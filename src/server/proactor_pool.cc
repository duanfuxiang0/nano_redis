#include "server/proactor_pool.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "server/sharding.h"
#include "protocol/resp_parser.h"
#include "command/command_registry.h"
#include "core/util.h"

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

ProactorPool::ProactorPool(size_t num_vcpus_value, uint16_t port_value) : num_vcpus(num_vcpus_value), port(port_value) {
	threads.reserve(num_vcpus);
	vcpus.resize(num_vcpus, nullptr);
	servers.resize(num_vcpus, nullptr);
}

ProactorPool::~ProactorPool() {
	Stop();
	Join();
}

bool ProactorPool::Start() {
	running = true;
	init_failed = false;
	init_done_vcpus.store(0);
	init_ok_vcpus.store(0);

	shard_set = std::make_unique<EngineShardSet>(num_vcpus);

	for (size_t i = 0; i < num_vcpus; ++i) {
		threads.emplace_back(&ProactorPool::VcpuMain, this, i);
	}

	// IMPORTANT: Don't block the OS thread here (std::condition_variable::wait),
	// otherwise Photon signal handling (signalfd) can't run and Ctrl-C won't work.
	while (init_done_vcpus.load() < num_vcpus && running.load()) {
		photon::thread_usleep(1000); // 1ms
	}

	if (!running.load() || init_failed.load() || init_ok_vcpus.load() != num_vcpus) {
		LOG_ERROR("ProactorPool failed to start: ok=`/` done=`/` port=`", init_ok_vcpus.load(), num_vcpus,
		          init_done_vcpus.load(), num_vcpus, port);
		Stop();
		Join();
		return false;
	}

	LOG_INFO("ProactorPool started with ` vCPUs on port `", num_vcpus, port);
	return true;
}

void ProactorPool::Stop() {
	running = false;

	// NOTE: We must be able to stop even if the per-shard TaskQueue hasn't started
	// (e.g. bind/listen failed). So we terminate servers directly.
	for (size_t i = 0; i < num_vcpus; ++i) {
		photon::net::ISocketServer* server = servers[i];
		if (server) {
			server->terminate();
		}
	}

	if (shard_set) {
		shard_set->Stop();
	}
}

void ProactorPool::Join() {
	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
	threads.clear();
}

photon::vcpu_base* ProactorPool::GetVcpu(size_t index) {
	if (index < vcpus.size()) {
		return vcpus[index];
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
		init_done_vcpus.fetch_add(1);
		if (ok) {
			init_ok_vcpus.fetch_add(1);
		} else {
			init_failed.store(true);
		}
	};

	photon::PhotonOptions photon_options;
	photon_options.use_pooled_stack_allocator = true;

	int ret =
	    photon::init(photon::INIT_EVENT_IOURING | photon::INIT_EVENT_SIGNAL, photon::INIT_IO_NONE, photon_options);
	if (ret < 0) {
		LOG_WARN("vCPU `: Failed to initialize io_uring, falling back to epoll", vcpu_index);
		ret = photon::init(photon::INIT_EVENT_EPOLL | photon::INIT_EVENT_SIGNAL, photon::INIT_IO_NONE, photon_options);
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

	vcpus[vcpu_index] = photon::get_vcpu();

	EngineShard* shard = shard_set->GetShard(vcpu_index);
	shard->InitializeInThread();

	photon::net::ISocketServer* server =
	    FLAGS_use_iouring_tcp_server ? photon::net::new_iouring_tcp_server() : photon::net::new_tcp_socket_server();
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
	servers[vcpu_index] = server;
	DEFER(servers[vcpu_index] = nullptr);

	ret = server->setsockopt<int>(SOL_SOCKET, SO_REUSEPORT, 1);
	if (ret < 0) {
		LOG_ERROR("vCPU `: Failed to set SO_REUSEPORT", vcpu_index);
		report_init(false);
		return;
	}

	if (server->bind_v4any(port) < 0) {
		LOG_ERROR("vCPU `: Failed to bind port `", vcpu_index, port);
		report_init(false);
		return;
	}

	if (server->listen(128) < 0) {
		LOG_ERROR("vCPU `: Failed to listen", vcpu_index);
		report_init(false);
		return;
	}

	LOG_INFO("vCPU ` (Shard `) listening on port ` with SO_REUSEPORT", vcpu_index, vcpu_index, port);

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

	EngineShard* local_shard = EngineShard::Tlocal();
	size_t vcpu_index = local_shard->ShardId();

	RESPParser parser(stream);

	std::vector<NanoObj> args;
	args.reserve(8);
	std::vector<NanoObj> forwarded_args;
	forwarded_args.reserve(8);

	while (running) {
		args.clear();
		int ret = parser.ParseCommand(args);

		if (ret < 0) {
			return 0;
		}

		if (args.empty()) {
			continue;
		}

		const std::string_view cmd_sv = args[0].GetStringView();
		if (!cmd_sv.empty() && EqualsIgnoreCase(cmd_sv, "QUIT")) {
			const std::string& ok = RESPParser::OkResponse();
			stream->send(ok.data(), ok.size());
			photon::thread_usleep(10000);
			return 0;
		}

		// IMPORTANT:
		// Route requests to the owning shard based on the key. For same-shard requests, we can
		// execute directly on the current vCPU (fast path). For cross-shard requests, we hop via
		// TaskQueue to preserve shard ownership.
		std::string response;

		size_t target_shard = vcpu_index;
		if (args.size() >= 2) {
			// NOTE: keys may be stored as INT_TAG internally, so don't use getStringView() here.
			// Always hash using the string representation.
			const std::string key = args[1].ToString();
			target_shard = Shard(key, num_vcpus);
		}

		if (target_shard == vcpu_index) {
			CommandContext ctx(local_shard, shard_set.get(), num_vcpus, local_shard->GetDB().CurrentDB());
			response = CommandRegistry::Instance().Execute(args, &ctx);
		} else {
			// Avoid per-command heap churn:
			// - Keep `args` capacity stable for parsing
			// - Keep `forwarded_args` buffer stable across requests
			// - Pass args by reference since Await() is synchronous
			forwarded_args.clear();
			forwarded_args.swap(args);

			response = shard_set->Await(target_shard, [this, &forwarded_args]() -> std::string {
				EngineShard* shard = EngineShard::Tlocal();
				if (shard == nullptr) {
					return RESPParser::MakeError("ERR internal shard context");
				}
				CommandContext ctx(shard, shard_set.get(), num_vcpus, shard->GetDB().CurrentDB());
				return CommandRegistry::Instance().Execute(forwarded_args, &ctx);
			});
			forwarded_args.clear();
		}

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
