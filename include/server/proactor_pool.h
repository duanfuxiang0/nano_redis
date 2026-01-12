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

// ProactorPool 管理 N 个 vCPU，每个 vCPU 拥有一个分片
class ProactorPool {
public:
	explicit ProactorPool(size_t num_vcpus, uint16_t port);
	~ProactorPool();

	ProactorPool(const ProactorPool&) = delete;
	ProactorPool& operator=(const ProactorPool&) = delete;

public:
	void Start();
	void Stop();
	void Join();

	size_t size() const { return num_vcpus_; }
	EngineShardSet* GetShardSet() { return shard_set_.get(); }
	photon::vcpu_base* GetVcpu(size_t index);

private:
	void VcpuMain(size_t vcpu_index);
	int HandleConnection(photon::net::ISocketStream* stream);

private:
	size_t num_vcpus_;
	uint16_t port_;
	std::atomic<bool> running_{false};

	std::vector<std::thread> threads_;
	std::vector<photon::vcpu_base*> vcpus_;
	std::vector<photon::net::ISocketServer*> servers_;
	std::unique_ptr<EngineShardSet> shard_set_;

	std::mutex vcpu_mutex_;
	std::condition_variable vcpu_cv_;
	std::atomic<size_t> ready_vcpus_{0};
};
