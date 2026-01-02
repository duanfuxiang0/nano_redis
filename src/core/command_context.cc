#include "core/command_context.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"

Database* CommandContext::GetDB() const {
	if (local_shard) return &local_shard->GetDB();
	return legacy_db;
}

Database* CommandContext::GetShardDB(size_t shard_id) const {
	if (!shard_set) return nullptr;
	auto* shard = shard_set->GetShard(shard_id);
	if (!shard) return nullptr;
	return &shard->GetDB();
}
