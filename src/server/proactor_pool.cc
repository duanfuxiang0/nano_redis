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
#include <unistd.h>
#include <string_view>

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

void ProactorPool::Start() {
	running_ = true;

	shard_set_ = std::make_unique<EngineShardSet>(num_vcpus_);

	for (size_t i = 0; i < num_vcpus_; ++i) {
		threads_.emplace_back(&ProactorPool::VcpuMain, this, i);
	}

	{
		std::unique_lock<std::mutex> lock(vcpu_mutex_);
		vcpu_cv_.wait(lock, [this]() { return ready_vcpus_ >= num_vcpus_; });
	}

	LOG_INFO("ProactorPool started with ` vCPUs on port `", num_vcpus_, port_);
}

void ProactorPool::Stop() {
	if (!running_) {
		return;
	}

	running_ = false;

	if (shard_set_) {
		for (size_t i = 0; i < num_vcpus_; ++i) {
			photon::net::ISocketServer* server = servers_[i];
			shard_set_->Add(i, [server]() {
				if (server) {
					server->terminate();
				}
			});
		}
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
	int ret = photon::init(
		photon::INIT_EVENT_IOURING | photon::INIT_EVENT_SIGNAL,
		photon::INIT_IO_NONE
	);
	if (ret < 0) {
		LOG_WARN("vCPU `: Failed to initialize io_uring, falling back to epoll", vcpu_index);
		ret = photon::init(
			photon::INIT_EVENT_EPOLL | photon::INIT_EVENT_SIGNAL,
			photon::INIT_IO_NONE
		);
		if (ret < 0) {
			LOG_ERROR("vCPU `: Failed to initialize photon", vcpu_index);
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

	auto* server = photon::net::new_tcp_socket_server();
	if (server == nullptr) {
		LOG_ERROR("vCPU `: Failed to create socket server", vcpu_index);
		return;
	}
	DEFER(delete server);
	servers_[vcpu_index] = server;
	DEFER(servers_[vcpu_index] = nullptr);

	ret = server->setsockopt<int>(SOL_SOCKET, SO_REUSEPORT, 1);
	if (ret < 0) {
		LOG_ERROR("vCPU `: Failed to set SO_REUSEPORT", vcpu_index);
		return;
	}

	if (server->bind_v4any(port_) < 0) {
		LOG_ERROR("vCPU `: Failed to bind port `", vcpu_index, port_);
		return;
	}

	if (server->listen(128) < 0) {
		LOG_ERROR("vCPU `: Failed to listen", vcpu_index);
		return;
	}

	LOG_INFO("vCPU ` (Shard `) listening on port ` with SO_REUSEPORT",
		vcpu_index, vcpu_index, port_);

	shard->GetTaskQueue()->Start("shard-" + std::to_string(vcpu_index));

	{
		std::lock_guard<std::mutex> lock(vcpu_mutex_);
		++ready_vcpus_;
	}
	vcpu_cv_.notify_all();

	server->set_handler({this, &ProactorPool::HandleConnection});

	server->start_loop(true);

	shard->GetTaskQueue()->Shutdown();
}

int ProactorPool::HandleConnection(photon::net::ISocketStream* stream) {
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

		std::string response;

		if (args.size() >= 2) {
			std::string_view key = args[1].getStringView();
			size_t target_shard = Shard(key, num_vcpus_);

			if (target_shard == vcpu_index) {
				CommandContext ctx(local_shard, shard_set_.get(), num_vcpus_,
					local_shard->GetDB().CurrentDB());
				response = CommandRegistry::instance().execute(args, &ctx);
			} else {
				std::vector<NanoObj> forwarded_args;
				forwarded_args.swap(args);
				response = shard_set_->Await(target_shard, [this, forwarded_args = std::move(forwarded_args), target_shard]() mutable {
					EngineShard* target = shard_set_->GetShard(target_shard);
					CommandContext ctx(target, shard_set_.get(), num_vcpus_,
						target->GetDB().CurrentDB());
					return CommandRegistry::instance().execute(forwarded_args, &ctx);
				});
				if (args.capacity() < 8) {
					args.reserve(8);
				}
			}
		} else {
			CommandContext ctx(local_shard, shard_set_.get(), num_vcpus_,
				local_shard->GetDB().CurrentDB());
			response = CommandRegistry::instance().execute(args, &ctx);
		}

		ssize_t written = stream->send(response.data(), response.size());
		if (written < 0) {
			LOG_ERRNO_RETURN(0, -1, "Failed to write response");
		}
	}

	return 0;
}
