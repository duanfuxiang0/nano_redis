#pragma once

#include <cstddef>

class EngineShard;
class EngineShardSet;
class Database;

struct CommandContext {
	EngineShard* local_shard = nullptr;
	EngineShardSet* shard_set = nullptr;
	size_t shard_count = 1;
	size_t db_index = 0;
	void* connection = nullptr;
	Database* legacy_db = nullptr;

	CommandContext() = default;

	explicit CommandContext(Database* database, size_t index = 0)
		: legacy_db(database), db_index(index), shard_count(1) {}

	CommandContext(EngineShard* shard, EngineShardSet* shard_set, size_t shard_count, size_t db_index = 0)
		: local_shard(shard), shard_set(shard_set), shard_count(shard_count), db_index(db_index) {}

	Database* GetDB() const;
	size_t GetDBIndex() const { return db_index; }
	size_t GetShardCount() const { return shard_count; }

	bool IsSingleShard() const { return shard_count <= 1; }

	Database* GetShardDB(size_t shard_id) const;
};
