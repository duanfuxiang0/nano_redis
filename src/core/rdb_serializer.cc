#include "core/rdb_serializer.h"

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <utility>

namespace {

constexpr uint16_t kDbCount = 16;

uint32_t BuildCrc32(uint32_t crc, const uint8_t* data, size_t len) {
	uint32_t c = crc ^ 0xFFFFFFFFu;
	for (size_t i = 0; i < len; i++) {
		c ^= static_cast<uint32_t>(data[i]);
		for (int k = 0; k < 8; k++) {
			const uint32_t mask = -(c & 1u);
			c = (c >> 1) ^ (0xEDB88320u & mask);
		}
	}
	return c ^ 0xFFFFFFFFu;
}

uint64_t NowMs() {
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

uint32_t UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len) {
	return BuildCrc32(crc, data, len);
}

using HashType = ankerl::unordered_dense::map<std::string, std::string, ankerl::unordered_dense::hash<std::string>>;
using SetType = ankerl::unordered_dense::set<std::string, ankerl::unordered_dense::hash<std::string>>;

}  // namespace

RdbSerializer::RdbSerializer(io::Sink* sink, uint32_t shard_id, uint32_t num_shards)
    : sink_(sink), shard_id_(shard_id), num_shards_(num_shards) {}

std::error_code RdbSerializer::WriteRaw(const uint8_t* buf, size_t len) {
	if (sink_ == nullptr) {
		return std::make_error_code(std::errc::bad_file_descriptor);
	}
	if (len == 0) {
		return {};
	}
	auto ec = sink_->Append(buf, len);
	if (ec) {
		return ec;
	}
	checksum_ = UpdateCrc32(checksum_, buf, len);
	return {};
}

std::error_code RdbSerializer::WriteOpcode(uint8_t opcode) {
	return WriteRaw(&opcode, 1);
}

std::error_code RdbSerializer::SaveLen(uint64_t len) {
	uint8_t buf[10];
	size_t i = 0;
	while (true) {
		uint8_t b = static_cast<uint8_t>(len & 0x7F);
		len >>= 7;
		if (len != 0) {
			b |= 0x80;
		}
		buf[i++] = b;
		if (len == 0) {
			break;
		}
	}
	return WriteRaw(buf, i);
}

std::error_code RdbSerializer::SaveString(std::string_view val) {
	auto ec = SaveLen(val.size());
	if (ec) {
		return ec;
	}
	return WriteRaw(reinterpret_cast<const uint8_t*>(val.data()), val.size());
}

std::error_code RdbSerializer::SaveIntObject(const NanoObj& obj) {
	const int64_t value = obj.AsInt();
	const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
	return WriteRaw(p, sizeof(value));
}

std::error_code RdbSerializer::SaveHashObject(const NanoObj& obj) {
	const auto* hash = obj.GetObj<HashType>();
	if (hash == nullptr) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	auto ec = SaveLen(hash->size());
	if (ec) {
		return ec;
	}
	for (const auto& [field, value] : *hash) {
		ec = SaveString(field);
		if (ec) {
			return ec;
		}
		ec = SaveString(value);
		if (ec) {
			return ec;
		}
	}
	return {};
}

std::error_code RdbSerializer::SaveSetObject(const NanoObj& obj) {
	const auto* set = obj.GetObj<SetType>();
	if (set == nullptr) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	auto ec = SaveLen(set->size());
	if (ec) {
		return ec;
	}
	for (const auto& member : *set) {
		ec = SaveString(member);
		if (ec) {
			return ec;
		}
	}
	return {};
}

std::error_code RdbSerializer::SaveListObject(const NanoObj& obj) {
	const auto* list = obj.GetObj<std::deque<NanoObj>>();
	if (list == nullptr) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	auto ec = SaveLen(list->size());
	if (ec) {
		return ec;
	}
	for (const auto& item : *list) {
		std::string str;
		if (item.IsString()) {
			str = std::string(item.GetStringView());
		} else {
			str = item.ToString();
		}
		ec = SaveString(str);
		if (ec) {
			return ec;
		}
	}
	return {};
}

std::error_code RdbSerializer::SaveZsetObject(const NanoObj& obj) {
	(void)obj;
	return std::make_error_code(std::errc::not_supported);
}

std::error_code RdbSerializer::SaveObjectTypeOpcode(const NanoObj& obj) {
	if (obj.IsInt()) {
		return WriteOpcode(NRDB_OBJ_INT);
	}
	uint8_t type = obj.GetType();
	switch (type) {
		case OBJ_STRING:
			return WriteOpcode(NRDB_OBJ_STRING);
		case OBJ_HASH:
			return WriteOpcode(NRDB_OBJ_HASH);
		case OBJ_SET:
			return WriteOpcode(NRDB_OBJ_SET);
		case OBJ_LIST:
			return WriteOpcode(NRDB_OBJ_LIST);
		case OBJ_ZSET:
			return WriteOpcode(NRDB_OBJ_ZSET);
		default:
			return std::make_error_code(std::errc::invalid_argument);
	}
}

std::error_code RdbSerializer::SaveObjectData(const NanoObj& obj) {
	if (obj.IsInt()) {
		return SaveIntObject(obj);
	}
	uint8_t type = obj.GetType();
	switch (type) {
		case OBJ_STRING:
			return SaveString(obj.GetStringView());
		case OBJ_HASH:
			return SaveHashObject(obj);
		case OBJ_SET:
			return SaveSetObject(obj);
		case OBJ_LIST:
			return SaveListObject(obj);
		case OBJ_ZSET:
			return SaveZsetObject(obj);
		default:
			return std::make_error_code(std::errc::invalid_argument);
	}
}

std::error_code RdbSerializer::SaveHeader() {
	checksum_ = 0;
	auto ec = WriteRaw(kNrdbMagic.data(), kNrdbMagic.size());
	if (ec) {
		return ec;
	}
	const uint32_t shard_count = num_shards_;
	const uint64_t ts = NowMs();
	const uint16_t num_dbs = kDbCount;
	if (auto err = WriteRaw(reinterpret_cast<const uint8_t*>(&shard_id_), sizeof(shard_id_))) {
		return err;
	}
	if (auto err = WriteRaw(reinterpret_cast<const uint8_t*>(&shard_count), sizeof(shard_count))) {
		return err;
	}
	if (auto err = WriteRaw(reinterpret_cast<const uint8_t*>(&ts), sizeof(ts))) {
		return err;
	}
	return WriteRaw(reinterpret_cast<const uint8_t*>(&num_dbs), sizeof(num_dbs));
}

std::error_code RdbSerializer::SaveSelectDb(uint32_t dbid) {
	if (dbid == last_dbid_) {
		return {};
	}
	auto ec = WriteOpcode(NRDB_OPCODE_DB_SELECT);
	if (ec) {
		return ec;
	}
	if (auto err = SaveLen(dbid)) {
		return err;
	}
	last_dbid_ = dbid;
	return {};
}

std::error_code RdbSerializer::SaveEntry(const NanoObj& key, const NanoObj& value, int64_t expire_ms, uint32_t dbid) {
	auto ec = SaveSelectDb(dbid);
	if (ec) {
		return ec;
	}
	if (expire_ms > 0) {
		ec = WriteOpcode(NRDB_OPCODE_EXPIRE_MS);
		if (ec) {
			return ec;
		}
		ec = SaveLen(static_cast<uint64_t>(expire_ms));
		if (ec) {
			return ec;
		}
	}
	ec = SaveObjectTypeOpcode(value);
	if (ec) {
		return ec;
	}
	ec = SaveString(key.GetStringView());
	if (ec) {
		return ec;
	}
	return SaveObjectData(value);
}

std::error_code RdbSerializer::SaveFooter() {
	auto ec = WriteOpcode(NRDB_OPCODE_EOF);
	if (ec) {
		return ec;
	}
	const uint32_t crc = checksum_;
	return WriteRaw(reinterpret_cast<const uint8_t*>(&crc), sizeof(crc));
}

uint32_t RdbSerializer::UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len) {
	return ::UpdateCrc32(crc, data, len);
}
