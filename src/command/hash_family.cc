#include "command/hash_family.h"
#include "core/command_context.h"
#include "protocol/resp_parser.h"
#include <cstdlib>
#include <sstream>

void HashFamily::Register(CommandRegistry* registry) {
	registry->register_command_with_context("HSET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HSet(args, ctx); });
	registry->register_command_with_context("HGET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HGet(args, ctx); });
	registry->register_command_with_context("HMSET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HMSet(args, ctx); });
	registry->register_command_with_context("HMGET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HMGet(args, ctx); });
	registry->register_command_with_context("HDEL", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HDel(args, ctx); });
	registry->register_command_with_context("HEXISTS", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HExists(args, ctx); });
	registry->register_command_with_context("HLEN", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HLen(args, ctx); });
	registry->register_command_with_context("HKEYS", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HKeys(args, ctx); });
	registry->register_command_with_context("HVALS", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HVals(args, ctx); });
	registry->register_command_with_context("HGETALL", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HGetAll(args, ctx); });
	registry->register_command_with_context("HINCRBY", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HIncrBy(args, ctx); });
	registry->register_command_with_context("HSTRLEN", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HStrLen(args, ctx); });
	registry->register_command_with_context("HRANDFIELD", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HRandField(args, ctx); });
	registry->register_command_with_context("HSCAN", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return HScan(args, ctx); });
}

std::string HashFamily::HSet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HSET key field value [field value ...]
	if (args.size() < 4 || (args.size() % 2) != 0) {
		return RESPParser::make_error("wrong number of arguments for HSET");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		if (hash_obj != nullptr && !hash_obj->isHash()) {
			db->Del(key);
		}
		auto hash_table = new HashType();
		NanoObj new_hash = NanoObj::fromHash();
		new_hash.setObj(hash_table);
		db->Set(key, std::move(new_hash));
		hash_obj = db->Find(key);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	for (size_t i = 2; i < args.size(); i += 2) {
		std::string field = args[i].toString();
		std::string value = args[i + 1].toString();
		(*hash_table)[field] = value;
	}

	return RESPParser::ok_response();
}

std::string HashFamily::HGet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HGET key field
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for HGET");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_null_bulk_string();
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[2].toString();

	auto it = hash_table->find(field);
	if (it == hash_table->end()) {
		return RESPParser::make_null_bulk_string();
	}

	return RESPParser::make_bulk_string(it->second);
}

std::string HashFamily::HMSet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HMSET key field value [field value ...] (deprecated alias)
	if (args.size() < 4 || (args.size() % 2) != 0) {
		return RESPParser::make_error("wrong number of arguments for HMSET");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		if (hash_obj != nullptr && !hash_obj->isHash()) {
			db->Del(key);
		}
		auto hash_table = new HashType();
		NanoObj new_hash = NanoObj::fromHash();
		new_hash.setObj(hash_table);
		db->Set(key, std::move(new_hash));
		hash_obj = db->Find(key);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	for (size_t i = 2; i < args.size(); i += 2) {
		std::string field = args[i].toString();
		std::string value = args[i + 1].toString();
		(*hash_table)[field] = value;
	}

	return RESPParser::ok_response();
}

std::string HashFamily::HMGet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HMGET key field [field ...]
	if (args.size() < 3) {
		return RESPParser::make_error("wrong number of arguments for HMGET");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		std::string result = RESPParser::make_array(args.size() - 2);
		for (size_t i = 2; i < args.size(); i++) {
			result += RESPParser::make_null_bulk_string();
		}
		return result;
	}

	auto hash_table = hash_obj->getObj<HashType>();

	std::string result = RESPParser::make_array(args.size() - 2);
	for (size_t i = 2; i < args.size(); i++) {
		std::string field = args[i].toString();
		auto it = hash_table->find(field);
		if (it != hash_table->end()) {
			result += RESPParser::make_bulk_string(it->second);
		} else {
			result += RESPParser::make_null_bulk_string();
		}
	}

	return result;
}

std::string HashFamily::HDel(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HDEL key field [field ...]
	if (args.size() < 3) {
		return RESPParser::make_error("wrong number of arguments for HDEL");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	int deleted = 0;
	for (size_t i = 2; i < args.size(); i++) {
		std::string field = args[i].toString();
		if (hash_table->erase(field)) {
			deleted++;
		}
	}

	return RESPParser::make_integer(deleted);
}

std::string HashFamily::HExists(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HEXISTS key field
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for HEXISTS");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[2].toString();

	return RESPParser::make_integer(hash_table->count(field) ? 1 : 0);
}

std::string HashFamily::HLen(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HLEN key
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HLEN");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();
	return RESPParser::make_integer(static_cast<int64_t>(hash_table->size()));
}

std::string HashFamily::HKeys(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HKEYS key
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HKEYS");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_array(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	std::string result = RESPParser::make_array(static_cast<int64_t>(hash_table->size()));
	for (const auto& pair : *hash_table) {
		result += RESPParser::make_bulk_string(pair.first);
	}

	return result;
}

std::string HashFamily::HVals(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HVALS key
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HVALS");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_array(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	std::string result = RESPParser::make_array(static_cast<int64_t>(hash_table->size()));
	for (const auto& pair : *hash_table) {
		result += RESPParser::make_bulk_string(pair.second);
	}

	return result;
}

std::string HashFamily::HGetAll(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HGETALL key
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HGETALL");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_array(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	std::string result = RESPParser::make_array(static_cast<int64_t>(hash_table->size() * 2));
	for (const auto& pair : *hash_table) {
		result += RESPParser::make_bulk_string(pair.first);
		result += RESPParser::make_bulk_string(pair.second);
	}

	return result;
}

std::string HashFamily::HIncrBy(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HINCRBY key field increment
	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for HINCRBY");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_error("WRONGTYPE Operation against a key holding the wrong kind of value");
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[2].toString();
	std::string increment_str = args[3].toString();
	int64_t increment;
	if (!ParseLongLong(increment_str, &increment)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto it = hash_table->find(field);
	if (it == hash_table->end()) {
		(*hash_table)[field] = std::to_string(increment);
		return RESPParser::make_bulk_string(std::to_string(increment));
	}

	int64_t current;
	try {
		current = std::stoll(it->second);
	} catch (...) {
		return RESPParser::make_error("hash value is not an integer");
	}

	current += increment;
	(*hash_table)[field] = std::to_string(current);

	return RESPParser::make_bulk_string(std::to_string(current));
}

std::string HashFamily::HScan(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HSCAN key cursor [MATCH ...] [COUNT ...] (simplified)
	if (args.size() < 3) {
		return RESPParser::make_error("wrong number of arguments for HSCAN");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_error("WRONGTYPE Operation against a key holding the wrong kind of value");
	}

	auto hash_table = hash_obj->getObj<HashType>();

	uint64_t cursor = 0;
	if (args.size() >= 3) {
		try {
			cursor = std::stoull(args[2].toString());
		} catch (...) {
			return RESPParser::make_error("invalid cursor");
		}
	}

	if (cursor != 0) {
		std::string result = RESPParser::make_array(2);
		result += RESPParser::make_bulk_string("0");
		result += RESPParser::make_array(0);
		return result;
	}

	std::vector<std::string> keys;
	for (const auto& pair : *hash_table) {
		keys.push_back(pair.first);
		keys.push_back(pair.second);
	}

	std::string result = RESPParser::make_array(2);
	result += RESPParser::make_bulk_string("0");
	result += RESPParser::make_array(static_cast<int64_t>(keys.size()));
	for (const auto& item : keys) {
		result += RESPParser::make_bulk_string(item);
	}
	return result;
}

std::string HashFamily::HStrLen(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HSTRLEN key field
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for HSTRLEN");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[2].toString();

	auto it = hash_table->find(field);
	if (it == hash_table->end()) {
		return RESPParser::make_integer(0);
	}

	return RESPParser::make_integer(static_cast<int64_t>(it->second.length()));
}

std::string HashFamily::HRandField(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// HRANDFIELD key [count]
	if (args.size() < 2 || args.size() > 3) {
		return RESPParser::make_error("wrong number of arguments for HRANDFIELD");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_null_bulk_string();
	}

	auto hash_table = hash_obj->getObj<HashType>();

	if (hash_table->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	if (args.size() == 2) {
		auto it = hash_table->begin();
		size_t offset = std::rand() % hash_table->size();
		std::advance(it, offset);
		return RESPParser::make_bulk_string(it->first);
	}

	int64_t count = 1;
	if (!ParseLongLong(args[2].toString(), &count) || count < 0) {
		return RESPParser::make_error("count is not a valid positive integer");
	}

	std::string result = RESPParser::make_array(count);
	for (int i = 0; i < count && !hash_table->empty(); i++) {
		auto it = hash_table->begin();
		size_t offset = std::rand() % hash_table->size();
		std::advance(it, offset);
		result += RESPParser::make_bulk_string(it->first);
		hash_table->erase(it);
	}

	return result;
}

bool HashFamily::ParseLongLong(const std::string& s, int64_t* out) {
	char* end;
	*out = std::strtoll(s.c_str(), &end, 10);

	if (end == s.c_str() || *end != '\0') {
		return false;
	}

	if (*out == LLONG_MAX || *out == LLONG_MIN) {
		return false;
	}

	return true;
}
