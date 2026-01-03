#include "server/sharded_server.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"
#include <photon/common/alog.h>
#include <photon/thread/st.h>

ShardedServer::ShardedServer(size_t num_shards, int port)
    : shard_set_(new EngineShardSet(num_shards, port)), num_shards_(num_shards), port_(port) {
	// Register commands
	StringFamily::Register(&CommandRegistry::instance());
	HashFamily::Register(&CommandRegistry::instance());
	SetFamily::Register(&CommandRegistry::instance());
	ListFamily::Register(&CommandRegistry::instance());
}

ShardedServer::~ShardedServer() {
	Term();
}

int ShardedServer::Run() {
	LOG_INFO("Starting ShardedServer with ", num_shards_, " shards on port ", port_);
	shard_set_->Start();

	LOG_INFO("ShardedServer running. Press Ctrl+C to stop.");

	while (true) {
		st_sleep(1);
	}

	return 0;
}

void ShardedServer::Term() {
	shard_set_->Stop();
	shard_set_->Join();
}
