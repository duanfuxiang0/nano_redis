#pragma once

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <string_view>

#include "core/nano_obj.h"
#include "core/rdb_defs.h"
#include "core/unordered_dense.h"

namespace io {

class Sink {
public:
	virtual ~Sink() = default;
	virtual std::error_code Append(const uint8_t* data, size_t len) = 0;
};

}  // namespace io

class RdbSerializer {
public:
	explicit RdbSerializer(io::Sink* sink, uint32_t shard_id = 0, uint32_t num_shards = 1);

	std::error_code SaveHeader();
	std::error_code SaveSelectDb(uint32_t dbid);
	std::error_code SaveEntry(const NanoObj& key, const NanoObj& value, int64_t expire_ms, uint32_t dbid);
	std::error_code SaveFooter();

private:
	std::error_code SaveString(std::string_view val);
	std::error_code SaveLen(uint64_t len);
	std::error_code SaveObjectTypeOpcode(const NanoObj& obj);
	std::error_code SaveObjectData(const NanoObj& obj);
	std::error_code SaveHashObject(const NanoObj& obj);
	std::error_code SaveSetObject(const NanoObj& obj);
	std::error_code SaveListObject(const NanoObj& obj);
	std::error_code SaveZsetObject(const NanoObj& obj);
	std::error_code SaveIntObject(const NanoObj& obj);

	std::error_code WriteOpcode(uint8_t opcode);
	std::error_code WriteRaw(const uint8_t* buf, size_t len);
	static uint32_t UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len);

	io::Sink* sink_;
	uint32_t shard_id_;
	uint32_t num_shards_;
	uint32_t last_dbid_ = UINT32_MAX;
	uint32_t checksum_ = 0;
};
