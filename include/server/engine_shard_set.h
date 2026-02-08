#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <atomic>

#include "server/engine_shard.h"
#include "core/task_queue.h"

// EngineShardSet 管理所有数据库分片
class EngineShardSet {
public:
	explicit EngineShardSet(size_t num_shards);
	~EngineShardSet();

	EngineShardSet(const EngineShardSet&) = delete;
	EngineShardSet& operator=(const EngineShardSet&) = delete;

public:
	// 执行函数并等待结果
	template <typename F>
	auto Await(size_t shard_id, F&& func) -> decltype(func()) {
		return shards[shard_id]->GetTaskQueue()->Await(std::forward<F>(func));
	}

	// 发送任务不等待
	template <typename F>
	void Add(size_t shard_id, F&& func) {
		shards[shard_id]->GetTaskQueue()->Add(std::forward<F>(func));
	}

	EngineShard* GetShard(size_t shard_id) {
		return shards[shard_id].get();
	}
	size_t Size() const {
		return shards.size();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	size_t size() const {
		return Size();
	}
	void Stop();

private:
	std::vector<std::unique_ptr<EngineShard>> shards;
	std::atomic<bool> running {true};
};
