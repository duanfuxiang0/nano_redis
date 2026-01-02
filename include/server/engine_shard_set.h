#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "server/engine_shard.h"
#include "core/task_queue.h"

class EngineShardSet {
public:
	explicit EngineShardSet(size_t num_shards, uint16_t port);
	~EngineShardSet();

	template<typename F>
	auto Await(size_t shard_id, F&& func) -> decltype(func()) {
		return shards_[shard_id]->GetTaskQueue()->Await(std::forward<F>(func));
	}

	template<typename F>
	void Add(size_t shard_id, F&& func) {
		shards_[shard_id]->GetTaskQueue()->Add(std::forward<F>(func));
	}

	EngineShard* GetShard(size_t shard_id) { return shards_[shard_id].get(); }
	size_t size() const { return shards_.size(); }

	void Start();
	void Stop();
	void Join();

private:
	std::vector<std::unique_ptr<EngineShard>> shards_;
};
