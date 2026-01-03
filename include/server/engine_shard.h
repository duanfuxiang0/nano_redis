#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <array>

#include "core/database.h"
#include "core/compact_obj.h"
#include "core/task_queue.h"
#include "core/command_context.h"
#include <photon/net/socket.h>
#include <photon/common/alog.h>

class EngineShardSet;

class EngineShard {
public:
	static const size_t kNumDBs = 16;
	using DbIndex = size_t;

	EngineShard(size_t shard_id, uint16_t port, EngineShardSet* shard_set);
	~EngineShard();

	Database& GetDB() { return *db_; }

	TaskQueue* GetTaskQueue() { return &task_queue_; }

	size_t shard_id() const { return shard_id_; }

	static EngineShard* tlocal() { return tlocal_shard_; }

	void Start();
	void Stop();
	void Join();

private:
	void EventLoop();
	int HandleConnection(photon::net::ISocketStream* stream);
	std::string ProcessCommand(const std::vector<CompactObj>& args);

	size_t shard_id_;
	std::unique_ptr<Database> db_;
	TaskQueue task_queue_;
	uint16_t port_;
	EngineShardSet* shard_set_;
	std::thread thread_;
	std::atomic<bool> running_;
	photon::net::ISocketServer* server_ = nullptr;
	static __thread EngineShard* tlocal_shard_;
};
