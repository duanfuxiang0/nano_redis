#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <photon/thread/thread.h>
#include <photon/net/socket.h>

class EngineShard;
class EngineShardSet;

/**
 * @brief ProactorPool manages N vCPUs (OS threads), each hosting one shard.
 * 
 * Architecture:
 *   - Each vCPU runs a Photon event loop
 *   - Each vCPU owns exactly one EngineShard
 *   - Each vCPU handles TCP connections (I/O) via SO_REUSEPORT
 *   - Connection Fibers act as coordinators, forwarding requests to correct shards
 * 
 * This implements the Dragonfly-style shared-nothing architecture where:
 *   - Data is never shared between threads (each shard is owned by one vCPU)
 *   - Cross-shard communication happens via message passing (TaskQueue)
 *   - I/O can be handled by any vCPU, but data operations go to the owning shard
 */
class ProactorPool {
public:
	/**
	 * @brief Construct a ProactorPool with specified number of vCPUs
	 * @param num_vcpus Number of vCPUs to create (typically = number of CPU cores)
	 * @param port TCP port to listen on
	 */
	explicit ProactorPool(size_t num_vcpus, uint16_t port);
	~ProactorPool();

	// Non-copyable
	ProactorPool(const ProactorPool&) = delete;
	ProactorPool& operator=(const ProactorPool&) = delete;

public:
	/**
	 * @brief Start all vCPUs and begin accepting connections
	 */
	void Start();

	/**
	 * @brief Stop all vCPUs gracefully
	 */
	void Stop();

	/**
	 * @brief Wait for all vCPUs to terminate
	 */
	void Join();

	/**
	 * @brief Get the number of vCPUs (and shards)
	 */
	size_t size() const { return num_vcpus_; }

	/**
	 * @brief Get the EngineShardSet for cross-shard operations
	 */
	EngineShardSet* GetShardSet() { return shard_set_.get(); }

	/**
	 * @brief Get the vCPU handle for a specific index
	 * Used for thread migration
	 */
	photon::vcpu_base* GetVcpu(size_t index);

private:
	/**
	 * @brief Main event loop for each vCPU
	 * @param vcpu_index The index of this vCPU (0 to N-1)
	 */
	void VcpuMain(size_t vcpu_index);

	/**
	 * @brief Handle incoming TCP connection
	 * Runs in a Connection Fiber (coordinator)
	 * Gets vcpu_index from the thread-local shard
	 */
	int HandleConnection(photon::net::ISocketStream* stream);

private:
	size_t num_vcpus_;
	uint16_t port_;
	std::atomic<bool> running_{false};

	std::vector<std::thread> threads_;
	std::vector<photon::vcpu_base*> vcpus_;
	std::vector<photon::net::ISocketServer*> servers_;
	std::unique_ptr<EngineShardSet> shard_set_;

	// Synchronization for startup
	std::mutex vcpu_mutex_;
	std::condition_variable vcpu_cv_;
	std::atomic<size_t> ready_vcpus_{0};
};
