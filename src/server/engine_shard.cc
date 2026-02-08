#include "server/engine_shard.h"

#include <photon/common/alog.h>

__thread EngineShard* EngineShard::tlocal_shard = nullptr;

EngineShard::EngineShard(size_t shard_id_value) : shard_id(shard_id_value), db(std::make_unique<Database>()) {
}

EngineShard::~EngineShard() = default;

void EngineShard::InitializeInThread() {
	tlocal_shard = this;
	LOG_INFO("EngineShard initialized in thread", shard_id);
}
