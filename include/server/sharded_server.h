#pragma once

#include <memory>
#include "server/engine_shard_set.h"

class ShardedServer {
public:
	ShardedServer(size_t num_shards, int port);
	~ShardedServer();

	int Run();
	void Term();

private:
	std::unique_ptr<EngineShardSet> shard_set_;
	size_t num_shards_;
	int port_;
};
