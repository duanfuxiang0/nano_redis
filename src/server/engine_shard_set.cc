#include "server/engine_shard_set.h"

EngineShardSet::EngineShardSet(size_t num_shards, uint16_t port) {
	for (size_t i = 0; i < num_shards; ++i) {
		shards_.emplace_back(new EngineShard(i, port, this));
	}
}

EngineShardSet::~EngineShardSet() {
	Stop();
	Join();
}

void EngineShardSet::Start() {
	for (auto& shard : shards_) {
		shard->Start();
	}
}

void EngineShardSet::Stop() {
	for (auto& shard : shards_) {
		shard->Stop();
	}
}

void EngineShardSet::Join() {
	for (auto& shard : shards_) {
		shard->Join();
	}
}
