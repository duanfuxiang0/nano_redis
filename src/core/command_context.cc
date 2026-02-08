#include "core/command_context.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"

Database* CommandContext::GetDB() const {
	if (local_shard) {
		return &local_shard->GetDB();
	}
	return legacy_db;
}

Database* CommandContext::GetShardDB(size_t shard_id) const {
	// Unsafe in multi-shard mode. Keep it only for legacy/unit-test code paths.
	if (!shard_set || shard_count <= 1) {
		return GetDB();
	}

	(void)shard_id;
	return nullptr;
}
