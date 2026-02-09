#include "core/command_context.h"
#include "core/database.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"

Database* CommandContext::GetDB() const {
	Database* db = nullptr;
	if (local_shard) {
		db = &local_shard->GetDB();
	} else {
		db = legacy_db;
	}

	if (db != nullptr && db->CurrentDB() != db_index) {
		(void)db->Select(db_index);
	}
	return db;
}

Database* CommandContext::GetShardDB(size_t shard_id) const {
	// Unsafe in multi-shard mode. Keep it only for legacy/unit-test code paths.
	if (!shard_set || shard_count <= 1) {
		return GetDB();
	}

	(void)shard_id;
	return nullptr;
}
