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
}  // namespace

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

	// Create the shard set first (without starting threads)
	shard_set_ = std::make_unique<EngineShardSet>(num_vcpus_);

	// Start all vCPU threads
	for (size_t i = 0; i < num_vcpus_; ++i) {
		threads_.emplace_back(&ProactorPool::VcpuMain, this, i);
	}

	// Wait for all vCPUs to be ready
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

	// Terminate accept loops from within each vCPU via its TaskQueue.
	// This ensures server->start_loop(true) can exit and threads can join.
	// Note: TaskQueue::Shutdown() is called in VcpuMain after the accept loop exits.
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
	// Initialize Photon environment for this vCPU
	// Try io_uring first for best performance, fallback to epoll
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

	// Store this vCPU handle
	vcpus_[vcpu_index] = photon::get_vcpu();

	// Initialize the shard for this vCPU
	EngineShard* shard = shard_set_->GetShard(vcpu_index);
	shard->InitializeInThread();

	// Create TCP server with SO_REUSEPORT for load balancing
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

	// Start the task queue consumer fibers (for receiving cross-shard requests)
	shard->GetTaskQueue()->Start("shard-" + std::to_string(vcpu_index));

	// Signal that this vCPU is ready
	{
		std::lock_guard<std::mutex> lock(vcpu_mutex_);
		++ready_vcpus_;
	}
	vcpu_cv_.notify_all();

	// Set connection handler - each connection runs in its own fiber (coordinator)
	// We use {this, &method} syntax to create a Delegate
	server->set_handler({this, &ProactorPool::HandleConnection});

	// Main accept loop - blocks here until server is terminated
	server->start_loop(true);

	// Shutdown the task queue consumer fibers before photon::fini()
	shard->GetTaskQueue()->Shutdown();
}

int ProactorPool::HandleConnection(photon::net::ISocketStream* stream) {
	// Get the local shard for this vCPU (set during VcpuMain initialization)
	EngineShard* local_shard = EngineShard::tlocal();
	size_t vcpu_index = local_shard->shard_id();

	// LOG_INFO("vCPU `: New client connected (Connection Fiber = Coordinator)", vcpu_index);

	RESPParser parser(stream);

	// Reuse args vector across commands to avoid repeated allocations
	std::vector<CompactObj> args;
	args.reserve(8);  // Pre-allocate for typical command size

	while (running_) {
		args.clear();  // Clear but keep capacity
		int ret = parser.parse_command(args);

		if (ret < 0) {
			// LOG_INFO("vCPU `: Client disconnected", vcpu_index);
			return 0;
		}

		if (args.empty()) {
			continue;
		}
		
		// Handle QUIT command
		const std::string_view cmd_sv = args[0].getStringView();
		if (!cmd_sv.empty() && EqualsIgnoreCase(cmd_sv, "QUIT")) {
			std::string response = "+OK\r\n";
			stream->send(response.data(), response.size());
			photon::thread_usleep(10000);
			return 0;
		}

		// Determine which shard should handle this command
		// For key-based commands, calculate target shard from key
		// For global commands (PING, DBSIZE, etc.), use local shard
		std::string response;
		
		if (args.size() >= 2) {
			// Key-based command - route to correct shard
			std::string_view key = args[1].getStringView();
			size_t target_shard = Shard(key, num_vcpus_);

			if (target_shard == vcpu_index) {
				// Lucky! The key belongs to our local shard - execute directly
				CommandContext ctx(local_shard, shard_set_.get(), num_vcpus_,
					local_shard->GetDB().CurrentDB());
				response = CommandRegistry::instance().execute(args, &ctx);
			} else {
				// Key belongs to another shard - forward request via TaskQueue
				// The Fiber will suspend here (not block the thread!)
				// Critical: avoid deep-copying `args` (would allocate/copy all strings).
				// Move the parsed args into the cross-shard task instead.
				std::vector<CompactObj> forwarded_args;
				forwarded_args.swap(args);
				response = shard_set_->Await(target_shard, [this, forwarded_args = std::move(forwarded_args), target_shard]() mutable {
					EngineShard* target = shard_set_->GetShard(target_shard);
					CommandContext ctx(target, shard_set_.get(), num_vcpus_,
						target->GetDB().CurrentDB());
					return CommandRegistry::instance().execute(forwarded_args, &ctx);
				});
				// Restore minimal capacity for the next parse without forcing a large realloc.
				if (args.capacity() < 8) {
					args.reserve(8);
				}
			}
		} else {
			// Global command - execute locally
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
