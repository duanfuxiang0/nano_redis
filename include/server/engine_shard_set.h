#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <atomic>

#include "server/engine_shard.h"
#include "core/task_queue.h"

/**
 * @brief EngineShardSet manages all database shards.
 * 
 * Provides cross-shard communication primitives:
 *   - Await(): Send a task to a shard and wait for result (fiber-friendly)
 *   - Add(): Send a task to a shard without waiting (fire-and-forget)
 * 
 * These methods use the TaskQueue for message passing between vCPUs,
 * ensuring no shared mutable state between threads.
 */
class EngineShardSet {
public:
	/**
	 * @brief Construct a shard set with the specified number of shards
	 * @param num_shards Number of shards to create
	 */
	explicit EngineShardSet(size_t num_shards);
	~EngineShardSet();

	// Non-copyable
	EngineShardSet(const EngineShardSet&) = delete;
	EngineShardSet& operator=(const EngineShardSet&) = delete;

public:
	/**
	 * @brief Execute a function on a specific shard and wait for result
	 * 
	 * This is the primary cross-shard communication mechanism.
	 * The calling fiber will suspend (not block the thread!) while waiting.
	 * 
	 * @param shard_id Target shard index
	 * @param func Function to execute on the target shard
	 * @return The result of func()
	 */
	template<typename F>
	auto Await(size_t shard_id, F&& func) -> decltype(func()) {
		return shards_[shard_id]->GetTaskQueue()->Await(std::forward<F>(func));
	}

	/**
	 * @brief Send a task to a specific shard without waiting
	 * 
	 * Use this for fire-and-forget operations or when you'll
	 * synchronize via other means (e.g., barriers).
	 * 
	 * @param shard_id Target shard index
	 * @param func Function to execute on the target shard
	 */
	template<typename F>
	void Add(size_t shard_id, F&& func) {
		shards_[shard_id]->GetTaskQueue()->Add(std::forward<F>(func));
	}

	/**
	 * @brief Get a shard by index
	 */
	EngineShard* GetShard(size_t shard_id) { return shards_[shard_id].get(); }

	/**
	 * @brief Get the number of shards
	 */
	size_t size() const { return shards_.size(); }

	/**
	 * @brief Signal all shards to stop processing
	 */
	void Stop();

private:
	std::vector<std::unique_ptr<EngineShard>> shards_;
	std::atomic<bool> running_{true};
};
