#include "core/rdb_loader.h"

#include <array>
#include <cstring>
#include <string>

#include "core/unordered_dense.h"

namespace {

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

uint32_t UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len) {
	return BuildCrc32(crc, data, len);
}

using HashType = ankerl::unordered_dense::map<std::string, std::string, ankerl::unordered_dense::hash<std::string>>;
using SetType = ankerl::unordered_dense::set<std::string, ankerl::unordered_dense::hash<std::string>>;

}  // namespace

RdbLoader::RdbLoader(io::Source* source, uint32_t expected_shard_id)
    : source_(source), expected_shard_id_(expected_shard_id) {}

uint32_t RdbLoader::UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len) {
	return ::UpdateCrc32(crc, data, len);
}

std::error_code RdbLoader::ReadRaw(uint8_t* buf, size_t len) {
	if (source_ == nullptr) {
		return std::make_error_code(std::errc::bad_file_descriptor);
	}
	if (len == 0) {
		return {};
	}
	auto ec = source_->Read(buf, len);
	if (ec) {
		return ec;
	}
	checksum_ = UpdateCrc32(checksum_, buf, len);
	return {};
}

std::error_code RdbLoader::ReadOpcode(uint8_t* opcode) {
	return ReadRaw(opcode, 1);
}

std::error_code RdbLoader::ReadLen(uint64_t* len) {
	*len = 0;
	uint64_t shift = 0;
	uint8_t byte = 0;
	size_t rounds = 0;
	while (true) {
		auto ec = ReadOpcode(&byte);
		if (ec) {
			return ec;
		}
		*len |= static_cast<uint64_t>(byte & 0x7F) << shift;
		if ((byte & 0x80) == 0) {
			break;
		}
		shift += 7;
		rounds++;
		if (rounds > 10) {
			return std::make_error_code(std::errc::invalid_argument);
		}
	}
	return {};
}

std::error_code RdbLoader::ReadString(std::string& out) {
	uint64_t len = 0;
	auto ec = ReadLen(&len);
	if (ec) {
		return ec;
	}
	out.resize(len);
	if (len == 0) {
		return {};
	}
	return ReadRaw(reinterpret_cast<uint8_t*>(out.data()), len);
}

std::error_code RdbLoader::ReadObject(uint8_t type, NanoObj* out) {
	switch (type) {
		case NRDB_OBJ_STRING: {
			std::string value;
			auto ec = ReadString(value);
			if (ec) {
				return ec;
			}
			*out = NanoObj::FromString(value);
			return {};
		}
		case NRDB_OBJ_INT: {
			int64_t value = 0;
			auto ec = ReadRaw(reinterpret_cast<uint8_t*>(&value), sizeof(value));
			if (ec) {
				return ec;
			}
			*out = NanoObj::FromInt(value);
			return {};
		}
		case NRDB_OBJ_HASH: {
			auto obj = NanoObj::FromHash();
			auto* table = new HashType();
			obj.SetObj(table);
			uint64_t count = 0;
			auto ec = ReadLen(&count);
			if (ec) {
				return ec;
			}
			for (uint64_t i = 0; i < count; i++) {
				std::string field;
				std::string value;
				ec = ReadString(field);
				if (ec) {
					return ec;
				}
				ec = ReadString(value);
				if (ec) {
					return ec;
				}
				(*table)[std::move(field)] = std::move(value);
			}
			*out = std::move(obj);
			return {};
		}
		case NRDB_OBJ_SET: {
			auto obj = NanoObj::FromSet();
			auto* set = new SetType();
			obj.SetObj(set);
			uint64_t count = 0;
			auto ec = ReadLen(&count);
			if (ec) {
				return ec;
			}
			for (uint64_t i = 0; i < count; i++) {
				std::string member;
				ec = ReadString(member);
				if (ec) {
					return ec;
				}
				(void)set->insert(std::move(member));
			}
			*out = std::move(obj);
			return {};
		}
		case NRDB_OBJ_LIST: {
			auto obj = NanoObj::FromList();
			auto* list = new std::deque<NanoObj>();
			obj.SetObj(list);
			uint64_t count = 0;
			auto ec = ReadLen(&count);
			if (ec) {
				return ec;
			}
			for (uint64_t i = 0; i < count; i++) {
				std::string member;
				ec = ReadString(member);
				if (ec) {
					return ec;
				}
				list->push_back(NanoObj::FromString(member));
			}
			*out = std::move(obj);
			return {};
		}
		case NRDB_OBJ_ZSET:
			return std::make_error_code(std::errc::not_supported);
		default:
			return std::make_error_code(std::errc::invalid_argument);
	}
}

std::error_code RdbLoader::LoadHeader() {
	std::array<uint8_t, kNrdbMagicSize> magic{};
	auto ec = ReadRaw(magic.data(), magic.size());
	if (ec) {
		return ec;
	}
	if (magic != kNrdbMagic) {
		return std::make_error_code(std::errc::invalid_argument);
	}

	uint32_t shard_id = 0;
	uint32_t num_shards = 0;
	uint64_t timestamp = 0;
	uint16_t num_dbs = 0;
	ec = ReadRaw(reinterpret_cast<uint8_t*>(&shard_id), sizeof(shard_id));
	if (ec) {
		return ec;
	}
	ec = ReadRaw(reinterpret_cast<uint8_t*>(&num_shards), sizeof(num_shards));
	if (ec) {
		return ec;
	}
	(void)num_shards;
	(void)timestamp;
	ec = ReadRaw(reinterpret_cast<uint8_t*>(&timestamp), sizeof(timestamp));
	if (ec) {
		return ec;
	}
	ec = ReadRaw(reinterpret_cast<uint8_t*>(&num_dbs), sizeof(num_dbs));
	if (ec) {
		return ec;
	}
	(void)num_dbs;
	if (shard_id != expected_shard_id_) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	return {};
}

std::error_code RdbLoader::LoadFooter() {
	const uint32_t computed_crc = checksum_;
	uint32_t stored_crc = 0;
	auto ec = source_->Read(reinterpret_cast<uint8_t*>(&stored_crc), sizeof(stored_crc));
	if (ec) {
		return ec;
	}
	if (computed_crc != stored_crc) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	return {};
}

std::error_code RdbLoader::Load(const EntryHandler& handler) {
	checksum_ = 0;
	auto ec = LoadHeader();
	if (ec) {
		return ec;
	}
	uint32_t dbid = 0;
	int64_t expire_ms = 0;
	bool has_expire = false;

	while (true) {
		uint8_t opcode = 0;
		auto err = ReadOpcode(&opcode);
		if (err) {
			return err;
		}

		if (opcode == NRDB_OPCODE_DB_SELECT) {
			uint64_t selected_db = 0;
			err = ReadLen(&selected_db);
			if (err) {
				return err;
			}
			dbid = static_cast<uint32_t>(selected_db);
			has_expire = false;
			continue;
		}
		if (opcode == NRDB_OPCODE_DB_SIZE) {
			uint64_t ignored = 0;
			err = ReadLen(&ignored);
			if (err) {
				return err;
			}
			continue;
		}
		if (opcode == NRDB_OPCODE_EXPIRE_MS) {
			uint64_t expire = 0;
			err = ReadLen(&expire);
			if (err) {
				return err;
			}
			expire_ms = static_cast<int64_t>(expire);
			has_expire = true;
			continue;
		}
		if (opcode == NRDB_OPCODE_EOF) {
			return LoadFooter();
		}

		std::string key;
		err = ReadString(key);
		if (err) {
			return err;
		}
		NanoObj value;
		err = ReadObject(opcode, &value);
		if (err) {
			return err;
		}
		int64_t entry_expire_ms = has_expire ? expire_ms : 0;
		has_expire = false;
		if (handler) {
			err = handler(dbid, NanoObj::FromString(key), value, entry_expire_ms);
			if (err) {
				return err;
			}
		}
	}
}
