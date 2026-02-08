#include "server/sharded_server.h"
#include "server/proactor_pool.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"

#include <photon/common/alog.h>
#include <photon/thread/thread.h>

ShardedServer::ShardedServer(size_t num_shards_value, uint16_t port_value)
    : num_shards(num_shards_value), port(port_value), running(false) {
	StringFamily::Register(&CommandRegistry::Instance());
	HashFamily::Register(&CommandRegistry::Instance());
	SetFamily::Register(&CommandRegistry::Instance());
	ListFamily::Register(&CommandRegistry::Instance());
}

ShardedServer::~ShardedServer() {
	Term();
}

int ShardedServer::Run() {
	LOG_INFO("Starting ShardedServer with ` shards on port `", num_shards, port);
	LOG_INFO("Architecture: Shared-Nothing (Dragonfly-style)");
	LOG_INFO("  - ` vCPUs, each owning one shard", num_shards);
	LOG_INFO("  - I/O distributed via SO_REUSEPORT");
	LOG_INFO("  - Cross-shard requests via TaskQueue message passing");

	proactor_pool = std::make_unique<ProactorPool>(num_shards, port);
	if (!proactor_pool->Start()) {
		LOG_ERROR("Failed to start ShardedServer on port `", port);
		Term();
		return -1;
	}

	running.store(true, std::memory_order_release);
	LOG_INFO("ShardedServer running. Press Ctrl+C to stop.");

	while (running.load(std::memory_order_acquire)) {
		photon::thread_usleep(100000);
	}

	LOG_INFO("ShardedServer shutting down...");
	return 0;
}

void ShardedServer::Stop() {
	running.store(false, std::memory_order_release);
}

void ShardedServer::Term() {
	running.store(false, std::memory_order_release);
	if (proactor_pool) {
		proactor_pool->Stop();
		proactor_pool->Join();
		proactor_pool.reset();
	}
	LOG_INFO("ShardedServer terminated");
}
