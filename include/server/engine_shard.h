#pragma once

#include <memory>
#include <atomic>
#include <string>

#include "core/database.h"
#include "core/nano_obj.h"
#include "core/task_queue.h"
#include "core/command_context.h"

class EngineShardSet;

// EngineShard 表示一个数据库分片
// - 每个分片被一个 vCPU 线程独占
// - 其他线程通过 TaskQueue 通信
class EngineShard {
public:
	static const size_t kNumDBs = 16;
	using DbIndex = size_t;

	explicit EngineShard(size_t shard_id);
	~EngineShard();

	EngineShard(const EngineShard&) = delete;
	EngineShard& operator=(const EngineShard&) = delete;

public:
	// 仅限所属 vCPU 线程调用
	Database& GetDB() {
		return *db;
	}

	// 获取任务队列，用于跨分片请求
	TaskQueue* GetTaskQueue() {
		return &task_queue;
	}

	size_t ShardId() const {
		return shard_id;
	}

	// 获取当前线程的分片
	static EngineShard* Tlocal() {
		return tlocal_shard;
	}

	// 初始化线程本地指针
	void InitializeInThread();

private:
	size_t shard_id;
	std::unique_ptr<Database> db;
	TaskQueue task_queue;

	static __thread EngineShard* tlocal_shard;
};
