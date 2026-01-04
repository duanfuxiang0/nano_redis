#pragma once

#include <memory>
#include <atomic>
#include <string>

#include "core/database.h"
#include "core/compact_obj.h"
#include "core/task_queue.h"
#include "core/command_context.h"

class EngineShardSet;

/**
 * @brief EngineShard represents one database shard.
 * 
 * In the shared-nothing architecture:
 *   - Each shard is owned by exactly ONE vCPU (thread)
 *   - The owning thread can access the shard's data directly (no locks)
 *   - Other threads communicate via the TaskQueue (message passing)
 * 
 * The shard itself doesn't manage any threads - that's handled by ProactorPool.
 * This class is a pure data container with:
 *   - A Database instance for storing key-value data
 *   - A TaskQueue for receiving cross-shard requests
 */
class EngineShard {
public:
	static const size_t kNumDBs = 16;
	using DbIndex = size_t;

	/**
	 * @brief Construct an EngineShard
	 * @param shard_id The unique identifier for this shard (0 to N-1)
	 */
	explicit EngineShard(size_t shard_id);
	~EngineShard();

	// Non-copyable
	EngineShard(const EngineShard&) = delete;
	EngineShard& operator=(const EngineShard&) = delete;

public:
	/**
	 * @brief Get the Database owned by this shard
	 * IMPORTANT: Only call from the owning vCPU thread!
	 */
	Database& GetDB() { return *db_; }

	/**
	 * @brief Get the TaskQueue for this shard
	 * Used by other vCPUs to send work to this shard
	 */
	TaskQueue* GetTaskQueue() { return &task_queue_; }

	/**
	 * @brief Get the shard ID
	 */
	size_t shard_id() const { return shard_id_; }

	/**
	 * @brief Get the thread-local shard pointer
	 * Returns the shard owned by the current thread, or nullptr if none
	 */
	static EngineShard* tlocal() { return tlocal_shard_; }

	/**
	 * @brief Initialize the thread-local pointer
	 * Must be called from the owning vCPU thread during startup
	 */
	void InitializeInThread();

private:
	size_t shard_id_;
	std::unique_ptr<Database> db_;
	TaskQueue task_queue_;

	static __thread EngineShard* tlocal_shard_;
};
