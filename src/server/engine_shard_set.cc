#include "server/engine_shard_set.h"

#include <photon/common/alog.h>

EngineShardSet::EngineShardSet(size_t num_shards) {
	shards_.reserve(num_shards);
	for (size_t i = 0; i < num_shards; ++i) {
		shards_.emplace_back(new EngineShard(i));
	}
	LOG_INFO("EngineShardSet created with ` shards", num_shards);
}

EngineShardSet::~EngineShardSet() {
	Stop();
}

void EngineShardSet::Stop() {
	running_ = false;
}
