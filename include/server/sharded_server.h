#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>
#include <atomic>

class ProactorPool;

// ShardedServer 是分片 Redis 服务器的主入口
class ShardedServer {
public:
	ShardedServer(size_t num_shards, uint16_t port);
	~ShardedServer();

	ShardedServer(const ShardedServer&) = delete;
	ShardedServer& operator=(const ShardedServer&) = delete;

public:
	int Run();
	void Stop();
	void Term();

private:
	std::unique_ptr<ProactorPool> proactor_pool;
	size_t num_shards;
	uint16_t port;
	std::atomic<bool> running {false};
};
