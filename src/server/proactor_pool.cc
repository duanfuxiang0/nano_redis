#include "server/proactor_pool.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "server/sharding.h"
#include "protocol/resp_parser.h"
#include "command/command_registry.h"

#include <photon/photon.h>
#include <photon/thread/thread.h>
#include <photon/thread/thread11.h>
#include <photon/net/socket.h>
#include <photon/io/fd-events.h>
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string_view>
#include <gflags/gflags.h>

DECLARE_bool(tcp_nodelay);
DECLARE_bool(use_iouring_tcp_server);

namespace {
bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		unsigned char ca = static_cast<unsigned char>(a[i]);
		unsigned char cb = static_cast<unsigned char>(b[i]);
		unsigned char ua = (ca >= 'a' && ca <= 'z') ? static_cast<unsigned char>(ca - 'a' + 'A') : ca;
		unsigned char ub = (cb >= 'a' && cb <= 'z') ? static_cast<unsigned char>(cb - 'a' + 'A') : cb;
		if (ua != ub) {
			return false;
		}
	}
	return true;
}
}

ProactorPool::ProactorPool(size_t num_vcpus, uint16_t port)
	: num_vcpus_(num_vcpus), port_(port) {
	threads_.reserve(num_vcpus);
	vcpus_.resize(num_vcpus, nullptr);
	servers_.resize(num_vcpus, nullptr);
}

ProactorPool::~ProactorPool() {
	Stop();
	Join();
}

bool ProactorPool::Start() {
	running_ = true;
	init_failed_ = false;
	init_done_vcpus_.store(0);
	init_ok_vcpus_.store(0);

	shard_set_ = std::make_unique<EngineShardSet>(num_vcpus_);

	for (size_t i = 0; i < num_vcpus_; ++i) {
		threads_.emplace_back(&ProactorPool::VcpuMain, this, i);
	}

	// IMPORTANT: Don't block the OS thread here (std::condition_variable::wait),
	// otherwise Photon signal handling (signalfd) can't run and Ctrl-C won't work.
	while (init_done_vcpus_.load() < num_vcpus_ && running_.load()) {
		photon::thread_usleep(1000);  // 1ms
	}

	if (!running_.load() || init_failed_.load() || init_ok_vcpus_.load() != num_vcpus_) {
		LOG_ERROR("ProactorPool failed to start: ok=`/` done=`/` port=`",
			init_ok_vcpus_.load(), num_vcpus_, init_done_vcpus_.load(), num_vcpus_, port_);
		Stop();
		Join();
		return false;
	}

	LOG_INFO("ProactorPool started with ` vCPUs on port `", num_vcpus_, port_);
	return true;
}

void ProactorPool::Stop() {
	running_ = false;

	// NOTE: We must be able to stop even if the per-shard TaskQueue hasn't started
	// (e.g. bind/listen failed). So we terminate servers directly.
	for (size_t i = 0; i < num_vcpus_; ++i) {
		photon::net::ISocketServer* server = servers_[i];
		if (server) {
			server->terminate();
		}
	}

	if (shard_set_) {
		shard_set_->Stop();
	}
}

void ProactorPool::Join() {
	for (auto& thread : threads_) {
		if (thread.joinable()) {
			thread.join();
		}
	}
	threads_.clear();
}

photon::vcpu_base* ProactorPool::GetVcpu(size_t index) {
	if (index < vcpus_.size()) {
		return vcpus_[index];
	}
	return nullptr;
}

void ProactorPool::VcpuMain(size_t vcpu_index) {
	bool init_reported = false;
	auto report_init = [this, &init_reported](bool ok) {
		if (init_reported) {
			return;
		}
		init_reported = true;
		init_done_vcpus_.fetch_add(1);
		if (ok) {
			init_ok_vcpus_.fetch_add(1);
		} else {
			init_failed_.store(true);
		}
	};

	photon::PhotonOptions photon_options;
	photon_options.use_pooled_stack_allocator = true;

	int ret = photon::init(
		photon::INIT_EVENT_IOURING | photon::INIT_EVENT_SIGNAL,
		photon::INIT_IO_NONE,
		photon_options
	);
	if (ret < 0) {
		LOG_WARN("vCPU `: Failed to initialize io_uring, falling back to epoll", vcpu_index);
		ret = photon::init(
			photon::INIT_EVENT_EPOLL | photon::INIT_EVENT_SIGNAL,
			photon::INIT_IO_NONE,
			photon_options
		);
		if (ret < 0) {
			LOG_ERROR("vCPU `: Failed to initialize photon", vcpu_index);
			report_init(false);
			return;
		}
		LOG_INFO("vCPU ` initialized with epoll", vcpu_index);
	} else {
		LOG_INFO("vCPU ` initialized with io_uring", vcpu_index);
	}
	DEFER(photon::fini());

	vcpus_[vcpu_index] = photon::get_vcpu();

	EngineShard* shard = shard_set_->GetShard(vcpu_index);
	shard->InitializeInThread();

	photon::net::ISocketServer* server =
	    FLAGS_use_iouring_tcp_server ? photon::net::new_iouring_tcp_server()
	                                 : photon::net::new_tcp_socket_server();
	if (server == nullptr && FLAGS_use_iouring_tcp_server) {
		LOG_WARN("vCPU `: Failed to create io_uring TCP server, falling back to syscall TCP server", vcpu_index);
		server = photon::net::new_tcp_socket_server();
	}
	if (server == nullptr) {
		LOG_ERROR("vCPU `: Failed to create socket server", vcpu_index);
		report_init(false);
		return;
	}
	DEFER(delete server);
	servers_[vcpu_index] = server;
	DEFER(servers_[vcpu_index] = nullptr);

	ret = server->setsockopt<int>(SOL_SOCKET, SO_REUSEPORT, 1);
	if (ret < 0) {
		LOG_ERROR("vCPU `: Failed to set SO_REUSEPORT", vcpu_index);
		report_init(false);
		return;
	}

	if (server->bind_v4any(port_) < 0) {
		LOG_ERROR("vCPU `: Failed to bind port `", vcpu_index, port_);
		report_init(false);
		return;
	}

	if (server->listen(128) < 0) {
		LOG_ERROR("vCPU `: Failed to listen", vcpu_index);
		report_init(false);
		return;
	}

	LOG_INFO("vCPU ` (Shard `) listening on port ` with SO_REUSEPORT",
		vcpu_index, vcpu_index, port_);

	shard->GetTaskQueue()->Start("shard-" + std::to_string(vcpu_index));

	report_init(true);

	server->set_handler({this, &ProactorPool::HandleConnection});

	server->start_loop(true);

	shard->GetTaskQueue()->Shutdown();
}

int ProactorPool::HandleConnection(photon::net::ISocketStream* stream) {
	if (stream == nullptr) {
		return -1;
	}
	DEFER(stream->close());

	const int nodelay = FLAGS_tcp_nodelay ? 1 : 0;
	(void)stream->setsockopt(IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

	EngineShard* local_shard = EngineShard::tlocal();
	size_t vcpu_index = local_shard->shard_id();

	RESPParser parser(stream);

	std::vector<NanoObj> args;
	args.reserve(8);

	while (running_) {
		args.clear();
		int ret = parser.parse_command(args);

		if (ret < 0) {
			return 0;
		}

		if (args.empty()) {
			continue;
		}

		const std::string_view cmd_sv = args[0].getStringView();
		if (!cmd_sv.empty() && EqualsIgnoreCase(cmd_sv, "QUIT")) {
			std::string response = "+OK\r\n";
			stream->send(response.data(), response.size());
			photon::thread_usleep(10000);
			return 0;
		}

		// IMPORTANT:
		// In multi-shard mode we must serialize ALL DB accesses on the shard's TaskQueue,
		// otherwise multiple connection fibers can concurrently touch the same DB and corrupt
		// internal hash tables (we observed hangs inside ankerl::unordered_dense).
		std::string response;

		size_t target_shard = vcpu_index;
		if (args.size() >= 2) {
			// NOTE: keys may be stored as INT_TAG internally, so don't use getStringView() here.
			// Always hash using the string representation.
			const std::string key = args[1].toString();
			target_shard = Shard(key, num_vcpus_);
		}

		std::vector<NanoObj> forwarded_args;
		forwarded_args.swap(args);

		response = shard_set_->Await(
		    target_shard,
		    [this, forwarded_args = std::move(forwarded_args), target_shard]() mutable -> std::string {
			    (void)target_shard;
			    EngineShard* shard = EngineShard::tlocal();
			    if (shard == nullptr) {
				    return RESPParser::make_error("ERR internal shard context");
			    }
			    CommandContext ctx(shard, shard_set_.get(), num_vcpus_, shard->GetDB().CurrentDB());
			    return CommandRegistry::instance().execute(forwarded_args, &ctx);
		    });

		// Keep args buffer reasonably sized.
		if (args.capacity() < 8) {
			args.reserve(8);
		}

		ssize_t written = stream->send(response.data(), response.size());
		if (written < 0) {
			LOG_ERRNO_RETURN(0, -1, "Failed to write response");
		}
	}

	return 0;
}
