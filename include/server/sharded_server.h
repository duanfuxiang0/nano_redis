#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>

class ProactorPool;

/**
 * @brief ShardedServer is the main entry point for the sharded Redis server.
 * 
 * It uses a Dragonfly-style shared-nothing architecture:
 *   - N vCPUs (threads), each owning one database shard
 *   - I/O is handled by all vCPUs (via SO_REUSEPORT load balancing)
 *   - Connection Fibers act as coordinators, routing requests to correct shards
 *   - Cross-shard communication via message passing (no shared state)
 */
class ShardedServer {
public:
	/**
	 * @brief Construct a ShardedServer
	 * @param num_shards Number of shards (and vCPUs) to create
	 * @param port TCP port to listen on
	 */
	ShardedServer(size_t num_shards, uint16_t port);
	~ShardedServer();

	// Non-copyable
	ShardedServer(const ShardedServer&) = delete;
	ShardedServer& operator=(const ShardedServer&) = delete;

public:
	/**
	 * @brief Start the server and block until terminated
	 * @return 0 on success, non-zero on error
	 */
	int Run();

	/**
	 * @brief Stop the server gracefully
	 */
	void Term();

private:
	std::unique_ptr<ProactorPool> proactor_pool_;
	size_t num_shards_;
	uint16_t port_;
};
