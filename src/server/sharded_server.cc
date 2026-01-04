#include "server/sharded_server.h"
#include "server/proactor_pool.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"

#include <photon/common/alog.h>
#include <photon/thread/thread.h>

#include <csignal>
#include <atomic>

namespace {
std::atomic<bool> g_running{true};

void SignalHandler(int signum) {
	LOG_INFO("Received signal `, shutting down...", signum);
	g_running = false;
}
}  // namespace

ShardedServer::ShardedServer(size_t num_shards, uint16_t port)
	: num_shards_(num_shards), port_(port) {
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

	// Set up signal handlers
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);

	// Create and start the proactor pool
	proactor_pool_ = std::make_unique<ProactorPool>(num_shards_, port_);
	proactor_pool_->Start();

	LOG_INFO("ShardedServer running. Press Ctrl+C to stop.");

	// Wait for shutdown signal
	while (g_running) {
		photon::thread_usleep(100000);  // 100ms
	}

	LOG_INFO("ShardedServer shutting down...");
	return 0;
}

void ShardedServer::Term() {
	g_running = false;
	if (proactor_pool_) {
		proactor_pool_->Stop();
		proactor_pool_->Join();
		proactor_pool_.reset();
	}
	LOG_INFO("ShardedServer terminated");
}
