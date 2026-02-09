#include "server/proactor_pool.h"
#include "server/connection.h"
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
#include <algorithm>
#include <chrono>
#include <limits>
#include <absl/container/flat_hash_map.h>
#include <gflags/gflags.h>

DECLARE_bool(tcp_nodelay);
DECLARE_bool(use_iouring_tcp_server);

namespace {

constexpr size_t kPipelineFlushThresholdBytes = 16 * 1024;
constexpr uint64_t kActiveExpireIntervalUsec = 100 * 1000;
constexpr size_t kActiveExpireKeysPerDb = 32;

using ConnectionMap = absl::flat_hash_map<uint64_t, Connection*>;
thread_local ConnectionMap tlocal_connections;
std::atomic<int64_t> g_pause_until_ms {0};

int64_t CurrentTimeMs() {
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::milliseconds;
	return std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
}

void PauseIfNeeded() {
	for (;;) {
		const int64_t now_ms = CurrentTimeMs();
		const int64_t pause_until_ms = g_pause_until_ms.load(std::memory_order_relaxed);
		if (pause_until_ms <= now_ms) {
			return;
		}

		const uint64_t sleep_usec = static_cast<uint64_t>(pause_until_ms - now_ms) * 1000;
		photon::thread_usleep(sleep_usec);
	}
}

} // namespace

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

void ProactorPool::RegisterLocalConnection(Connection* connection) {
	if (connection == nullptr) {
		return;
	}
	tlocal_connections[connection->GetClientId()] = connection;
}

void ProactorPool::UnregisterLocalConnection(uint64_t client_id) {
	(void)tlocal_connections.erase(client_id);
}

std::vector<ProactorPool::ClientSnapshot> ProactorPool::ListLocalConnections() {
	const int64_t now_ms = CurrentTimeMs();
	std::vector<ClientSnapshot> snapshots;
	snapshots.reserve(tlocal_connections.size());
	for (const auto& [client_id, connection] : tlocal_connections) {
		if (connection == nullptr) {
			continue;
		}
		ClientSnapshot snapshot;
		snapshot.client_id = client_id;
		snapshot.db_index = connection->GetDBIndex();
		snapshot.client_name = connection->GetClientName();
		snapshot.last_command = connection->GetLastCommand();
		snapshot.age_sec = std::max<int64_t>(0, (now_ms - connection->GetConnectedAtMs()) / 1000);
		snapshot.idle_sec = std::max<int64_t>(0, (now_ms - connection->GetLastActiveAtMs()) / 1000);
		snapshot.close_requested = connection->IsCloseRequested();
		snapshots.push_back(std::move(snapshot));
	}
	return snapshots;
}

bool ProactorPool::KillLocalConnectionById(uint64_t client_id) {
	auto it = tlocal_connections.find(client_id);
	if (it == tlocal_connections.end() || it->second == nullptr) {
		return false;
	}
	it->second->RequestClose();
	return true;
}

void ProactorPool::PauseClients(uint64_t timeout_ms) {
	const int64_t now_ms = CurrentTimeMs();
	const int64_t new_pause_until_ms = timeout_ms > static_cast<uint64_t>(std::numeric_limits<int64_t>::max() - now_ms)
	                                       ? std::numeric_limits<int64_t>::max()
	                                       : now_ms + static_cast<int64_t>(timeout_ms);

	int64_t old_pause_until_ms = g_pause_until_ms.load(std::memory_order_relaxed);
	while (old_pause_until_ms < new_pause_until_ms &&
	       !g_pause_until_ms.compare_exchange_weak(old_pause_until_ms, new_pause_until_ms, std::memory_order_relaxed)) {
	}
}

int64_t ProactorPool::PauseUntilMs() {
	return g_pause_until_ms.load(std::memory_order_relaxed);
}

bool ProactorPool::IsPauseActive() {
	return PauseUntilMs() > CurrentTimeMs();
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

	photon::join_handle* expiry_handle = nullptr;
	if (auto* expiry_fiber = photon::thread_create11([this, shard]() {
		    while (running.load()) {
			    shard->GetDB().ActiveExpireCycle(kActiveExpireKeysPerDb);
			    photon::thread_usleep(kActiveExpireIntervalUsec);
		    }
	    })) {
		expiry_handle = photon::thread_enable_join(expiry_fiber);
	}
	DEFER(if (expiry_handle != nullptr) { photon::thread_join(expiry_handle); });

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

	Connection connection(stream);
	RegisterLocalConnection(&connection);
	DEFER(UnregisterLocalConnection(connection.GetClientId()));
	CommandRegistry& registry = CommandRegistry::Instance();

	std::vector<NanoObj> args;
	args.reserve(8);
	std::vector<NanoObj> forwarded_args;
	forwarded_args.reserve(8);

	while (running) {
		PauseIfNeeded();
		if (connection.IsCloseRequested()) {
			return 0;
		}

		args.clear();
		if (connection.ParseCommand(args) < 0) {
			return 0;
		}
		bool should_close = false;
		bool parse_error = false;
		while (running) {
			if (!args.empty()) {
				PauseIfNeeded();
				if (connection.IsCloseRequested()) {
					should_close = true;
					break;
				}

				const std::string_view cmd_sv = args[0].GetStringView();
				if (!cmd_sv.empty()) {
					connection.SetLastCommand(cmd_sv);
				} else {
					connection.SetLastCommand(args[0].ToString());
				}
				if (!cmd_sv.empty() && EqualsIgnoreCase(cmd_sv, "QUIT")) {
					connection.AppendResponse(RESPParser::OkResponse());
					should_close = true;
				} else {
					// IMPORTANT:
					// Route requests to the owning shard based on the key. For same-shard requests, we can
					// execute directly on the current vCPU (fast path). For cross-shard requests, we hop via
					// TaskQueue to preserve shard ownership.
					std::string response;
					size_t target_shard = vcpu_index;
					bool should_forward = false;

					const CommandRegistry::CommandMeta* meta = nullptr;
					if (!cmd_sv.empty()) {
						meta = registry.FindMeta(cmd_sv);
					} else {
						meta = registry.FindMeta(args[0].ToString());
					}

					if (meta != nullptr) {
						const bool is_no_key = (meta->flags & CommandRegistry::kCmdFlagNoKey) != 0;
						const bool is_multi_key = (meta->flags & CommandRegistry::kCmdFlagMultiKey) != 0;
						if (!is_no_key && !is_multi_key && meta->first_key > 0) {
							size_t first_key_index = static_cast<size_t>(meta->first_key);
							if (first_key_index < args.size()) {
								// NOTE: keys may be INT_TAG internally; hash via string form.
								const std::string key = args[first_key_index].ToString();
								target_shard = Shard(key, num_vcpus);
								should_forward = target_shard != vcpu_index;
							}
						}
					}

					if (!should_forward) {
						CommandContext ctx(local_shard, shard_set.get(), num_vcpus, connection.GetDBIndex(),
						                   &connection);
						response = registry.Execute(args, &ctx);
					} else {
						// Avoid per-command heap churn:
						// - Keep `args` capacity stable for parsing
						// - Keep `forwarded_args` buffer stable across requests
						// - Pass args by reference since Await() is synchronous
						forwarded_args.clear();
						forwarded_args.swap(args);
						const size_t conn_db_index = connection.GetDBIndex();

						response =
						    shard_set->Await(target_shard, [this, &forwarded_args, conn_db_index]() -> std::string {
							    EngineShard* shard = EngineShard::Tlocal();
							    if (shard == nullptr) {
								    return RESPParser::MakeError("ERR internal shard context");
							    }
							    CommandContext ctx(shard, shard_set.get(), num_vcpus, conn_db_index, nullptr);
							    return CommandRegistry::Instance().Execute(forwarded_args, &ctx);
						    });
						forwarded_args.clear();
					}

					connection.AppendResponse(response);
				}

				// Keep args buffer reasonably sized.
				if (args.capacity() < 8) {
					args.reserve(8);
				}
			}

			if (connection.PendingResponseBytes() >= kPipelineFlushThresholdBytes) {
				if (!connection.Flush()) {
					return -1;
				}
			}

			if (should_close) {
				break;
			}

			args.clear();
			RESPParser::TryParseResult try_parse_result = connection.TryParseCommandNoRead(args);
			if (try_parse_result == RESPParser::TryParseResult::OK) {
				continue;
			}
			if (try_parse_result == RESPParser::TryParseResult::ERROR) {
				parse_error = true;
			}
			break;
		}

		if (!connection.Flush()) {
			return -1;
		}
		if (should_close) {
			photon::thread_usleep(10000);
			return 0;
		}
		if (parse_error) {
			return 0;
		}
		if (connection.IsCloseRequested()) {
			return 0;
		}
	}

	return 0;
}
