#include "server/sharded_server.h"
#include "server/proactor_pool.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"
#include "command/server_family.h"
#include "core/database.h"
#include "core/rdb_loader.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "server/sharding.h"

#include <chrono>
#include <cstdint>
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <fstream>
#include <photon/thread/thread.h>
#include <system_error>
#include <string>

namespace {

constexpr char kDefaultDumpFile[] = "dump.nrdb";

int64_t CurrentTimeMs() {
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::milliseconds;
	return std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
}

class FileSource : public io::Source {
public:
	explicit FileSource(std::string file_path) : file_(file_path, std::ios::binary) {
	}

	std::error_code Read(uint8_t* data, size_t len) override {
		if (!file_.is_open()) {
			return std::make_error_code(std::errc::bad_file_descriptor);
		}
		if (len == 0) {
			return {};
		}

		file_.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(len));
		if (file_.gcount() != static_cast<std::streamsize>(len)) {
			return std::make_error_code(std::errc::io_error);
		}
		return {};
	}

	bool IsOpen() const {
		return file_.is_open();
	}

private:
	std::ifstream file_;
};

std::error_code LoadFromFile(const std::string& file_path, ProactorPool* pool, bool* loaded) {
	if (pool == nullptr) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	EngineShardSet* shard_set = pool->GetShardSet();
	if (shard_set == nullptr) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	if (loaded != nullptr) {
		*loaded = false;
	}

	FileSource source(file_path);
	if (!source.IsOpen()) {
		return {};
	}
	if (loaded != nullptr) {
		*loaded = true;
	}

	RdbLoader loader(&source);
	const size_t shard_count = shard_set->Size();

	return loader.Load([shard_set, shard_count](uint32_t dbid, const NanoObj& key, const NanoObj& value, int64_t expire_ms) {
		if (dbid >= Database::kNumDBs) {
			return std::make_error_code(std::errc::invalid_argument);
		}
		const size_t shard_id = Shard(key.GetStringView(), shard_count);
		return shard_set->Await(shard_id, [dbid, key, value, expire_ms]() -> std::error_code {
			                        EngineShard* shard = EngineShard::Tlocal();
			                        if (shard == nullptr) {
			                        	return std::make_error_code(std::errc::invalid_argument);
			                        }
			                        Database& db = shard->GetDB();
			                        if (!db.Select(dbid)) {
			                        	return std::make_error_code(std::errc::invalid_argument);
			                        }
			                        if (expire_ms > 0) {
			                        	const int64_t ttl_ms = expire_ms - CurrentTimeMs();
			                        	if (ttl_ms <= 0) {
			                        		return {};
			                        	}
			                        	db.Set(key, value);
			                        	(void)db.Expire(key, ttl_ms);
			                        	return {};
			                        }
			                        db.Set(key, value);
			                        return {};
		                        });
	});
}

} // namespace

ShardedServer::ShardedServer(size_t num_shards_value, uint16_t port_value)
    : num_shards(num_shards_value), port(port_value), running(false) {
	StringFamily::Register(&CommandRegistry::Instance());
	HashFamily::Register(&CommandRegistry::Instance());
	SetFamily::Register(&CommandRegistry::Instance());
	ListFamily::Register(&CommandRegistry::Instance());
	ServerFamily::Register(&CommandRegistry::Instance());
}

ShardedServer::~ShardedServer() {
	Term();
}

int ShardedServer::Run() {
	LOG_INFO("Starting ShardedServer with ` shards on port `", num_shards, port);
	LOG_INFO("Architecture: Shared-Nothing (Dragonfly-style)");
	LOG_INFO("  - ` vCPUs, each owning one shard", num_shards);
	LOG_INFO("  - I/O distributed via SO_REUSEPORT");
	LOG_INFO("  - Cross-shard requests via TaskQueue message passing");

	proactor_pool = std::make_unique<ProactorPool>(num_shards, port);
	if (!proactor_pool->Start()) {
		LOG_ERROR("Failed to start ShardedServer on port `", port);
		Term();
		return -1;
	}

	bool loaded = false;
	const auto ec = LoadFromFile(kDefaultDumpFile, proactor_pool.get(), &loaded);
	if (ec) {
		LOG_WARN("Failed to load persistence file '`' (`)", kDefaultDumpFile, ec.message());
	} else if (loaded) {
		LOG_INFO("Loaded persistence file '`'", kDefaultDumpFile);
	}

	running.store(true, std::memory_order_release);
	LOG_INFO("ShardedServer running. Press Ctrl+C to stop.");

	while (running.load(std::memory_order_acquire)) {
		photon::thread_usleep(100000);
	}

	LOG_INFO("ShardedServer shutting down...");
	return 0;
}

void ShardedServer::Stop() {
	running.store(false, std::memory_order_release);
}

void ShardedServer::Term() {
	running.store(false, std::memory_order_release);
	if (proactor_pool) {
		proactor_pool->Stop();
		proactor_pool->Join();
		proactor_pool.reset();
	}
	LOG_INFO("ShardedServer terminated");
}
