#pragma once

#include <cstddef>

class EngineShard;
class EngineShardSet;
class Database;
class Connection;

struct CommandContext {
	EngineShard* local_shard = nullptr;
	EngineShardSet* shard_set = nullptr;
	size_t shard_count = 1;
	size_t db_index = 0;
	Connection* connection = nullptr;
	Database* legacy_db = nullptr;

	CommandContext() = default;

	explicit CommandContext(Database* database, size_t index = 0, Connection* conn = nullptr)
	    : shard_count(1), db_index(index), connection(conn), legacy_db(database) {
	}

	CommandContext(EngineShard* shard, EngineShardSet* shard_set_value, size_t shard_count_value,
	               size_t db_index_value = 0, Connection* conn = nullptr)
	    : local_shard(shard), shard_set(shard_set_value), shard_count(shard_count_value), db_index(db_index_value),
	      connection(conn) {
	}

	Database* GetDB() const;
	size_t GetDBIndex() const {
		return db_index;
	}
	size_t GetShardCount() const {
		return shard_count;
	}

	bool IsSingleShard() const {
		return shard_count <= 1;
	}

	// 直接返回远程分片的 Database* 会破坏无共享架构, 使用 shard_set->Await/Add 在所属线程执行
	Database* GetShardDB(size_t shard_id) const;
};
