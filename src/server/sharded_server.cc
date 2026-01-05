#include "server/sharded_server.h"
#include "server/proactor_pool.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"

#include <photon/common/alog.h>
#include <photon/thread/thread.h>

ShardedServer::ShardedServer(size_t num_shards, uint16_t port)
	: num_shards_(num_shards), port_(port), running_(false) {
	// Register all command families
	StringFamily::Register(&CommandRegistry::instance());
	HashFamily::Register(&CommandRegistry::instance());
	SetFamily::Register(&CommandRegistry::instance());
	ListFamily::Register(&CommandRegistry::instance());
}

ShardedServer::~ShardedServer() {
	Term();
}

int ShardedServer::Run() {
	LOG_INFO("Starting ShardedServer with ` shards on port `", num_shards_, port_);
	LOG_INFO("Architecture: Shared-Nothing (Dragonfly-style)");
	LOG_INFO("  - ` vCPUs, each owning one shard", num_shards_);
	LOG_INFO("  - I/O distributed via SO_REUSEPORT");
	LOG_INFO("  - Cross-shard requests via TaskQueue message passing");

	// Create and start the proactor pool
	proactor_pool_ = std::make_unique<ProactorPool>(num_shards_, port_);
	proactor_pool_->Start();

	running_ = true;
	LOG_INFO("ShardedServer running. Press Ctrl+C to stop.");

	// Wait for shutdown signal (Stop() will set running_ to false)
	while (running_) {
		photon::thread_usleep(100000);  // 100ms
	}

	LOG_INFO("ShardedServer shutting down...");
	return 0;
}

void ShardedServer::Stop() {
	running_ = false;
}

void ShardedServer::Term() {
	running_ = false;
	if (proactor_pool_) {
		proactor_pool_->Stop();
		proactor_pool_->Join();
		proactor_pool_.reset();
	}
	LOG_INFO("ShardedServer terminated");
}
