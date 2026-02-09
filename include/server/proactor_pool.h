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
#include <string>

#include <photon/thread/thread.h>
#include <photon/net/socket.h>

class EngineShard;
class EngineShardSet;
class Connection;

// ProactorPool 管理 N 个 vCPU，每个 vCPU 拥有一个分片
class ProactorPool {
public:
	struct ClientSnapshot {
		uint64_t client_id = 0;
		size_t db_index = 0;
		std::string client_name;
		std::string last_command;
		int64_t age_sec = 0;
		int64_t idle_sec = 0;
		bool close_requested = false;
	};

	explicit ProactorPool(size_t num_vcpus, uint16_t port);
	~ProactorPool();

	ProactorPool(const ProactorPool&) = delete;
	ProactorPool& operator=(const ProactorPool&) = delete;

public:
	bool Start();
	void Stop();
	void Join();

	size_t Size() const {
		return num_vcpus;
	}
	EngineShardSet* GetShardSet() {
		return shard_set.get();
	}
	photon::vcpu_base* GetVcpu(size_t index);
	static void RegisterLocalConnection(Connection* connection);
	static void UnregisterLocalConnection(uint64_t client_id);
	static std::vector<ClientSnapshot> ListLocalConnections();
	static bool KillLocalConnectionById(uint64_t client_id);
	static void PauseClients(uint64_t timeout_ms);
	static int64_t PauseUntilMs();
	static bool IsPauseActive();

private:
	void VcpuMain(size_t vcpu_index);
	int HandleConnection(photon::net::ISocketStream* stream);

private:
	size_t num_vcpus;
	uint16_t port;
	std::atomic<bool> running {false};
	std::atomic<bool> init_failed {false};

	std::vector<std::thread> threads;
	std::vector<photon::vcpu_base*> vcpus;
	std::vector<photon::net::ISocketServer*> servers;
	std::unique_ptr<EngineShardSet> shard_set;

	std::atomic<size_t> init_done_vcpus {0};
	std::atomic<size_t> init_ok_vcpus {0};
};
