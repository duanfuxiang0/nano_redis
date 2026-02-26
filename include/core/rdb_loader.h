#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <system_error>

#include "core/nano_obj.h"
#include "core/rdb_defs.h"

namespace io {

class Source {
public:
	virtual ~Source() = default;
	virtual std::error_code Read(uint8_t* data, size_t len) = 0;
};

}  // namespace io

class RdbLoader {
public:
	using EntryHandler = std::function<std::error_code(uint32_t dbid, const NanoObj& key, const NanoObj& value, int64_t expire_ms)>;

	explicit RdbLoader(io::Source* source, uint32_t expected_shard_id = 0);

	std::error_code Load(const EntryHandler& handler);

private:
	std::error_code ReadRaw(uint8_t* buf, size_t len);
	std::error_code ReadOpcode(uint8_t* opcode);
	std::error_code ReadLen(uint64_t* len);
	std::error_code ReadString(std::string& out);
	std::error_code ReadObject(uint8_t type, NanoObj* out);
	std::error_code LoadHeader();
	std::error_code LoadFooter();
	uint32_t UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len);

	io::Source* source_;
	uint32_t expected_shard_id_;
	uint32_t checksum_ = 0;
};
