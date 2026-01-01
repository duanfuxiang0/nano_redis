#include "command/hash_family.h"
#include "server/server.h"
#include "protocol/resp_parser.h"
#include <cstdlib>
#include <sstream>

static Database* g_db = nullptr;

namespace {
	Database* GetDatabase() {
		return g_db;
	}
} // namespace

void HashFamily::SetDatabase(Database* db) {
	g_db = db;
}

void HashFamily::Register(CommandRegistry* registry) {
	registry->register_command("HSET", HSet);
	registry->register_command("HGET", HGet);
	registry->register_command("HMSET", HMSet);
	registry->register_command("HMGET", HMGet);
	registry->register_command("HDEL", HDel);
	registry->register_command("HEXISTS", HExists);
	registry->register_command("HLEN", HLen);
	registry->register_command("HKEYS", HKeys);
	registry->register_command("HVALS", HVals);
	registry->register_command("HGETALL", HGetAll);
	registry->register_command("HINCRBY", HIncrBy);
	registry->register_command("HSTRLEN", HStrLen);
	registry->register_command("HRANDFIELD", HRandField);
	registry->register_command("HSCAN", HScan);
}

std::string HashFamily::HSet(const std::vector<CompactObj>& args) {
	if (args.size() < 3 || (args.size() % 2) != 1) {
		return RESPParser::make_error("wrong number of arguments for HSET");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		if (hash_obj != nullptr && !hash_obj->isHash()) {
			db->Del(key);
		}
		auto hash_table = new HashType();
		CompactObj new_hash = CompactObj::fromHash();
		new_hash.setObj(hash_table);
		db->Set(key, std::move(new_hash));
		hash_obj = db->Find(key);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	for (size_t i = 1; i < args.size(); i += 2) {
		std::string field = args[i].toString();
		std::string value = args[i + 1].toString();
		(*hash_table)[field] = value;
	}

	return RESPParser::make_simple_string("OK");
}

std::string HashFamily::HGet(const std::vector<CompactObj>& args) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HGET");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_null_bulk_string();
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[1].toString();

	auto it = hash_table->find(field);
	if (it == hash_table->end()) {
		return RESPParser::make_null_bulk_string();
	}

	return RESPParser::make_bulk_string(it->second);
}

std::string HashFamily::HMSet(const std::vector<CompactObj>& args) {
	if (args.size() < 3 || args.size() % 2 != 1) {
		return RESPParser::make_error("wrong number of arguments for HMSET");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		if (hash_obj != nullptr && !hash_obj->isHash()) {
			db->Del(key);
		}
		auto hash_table = new HashType();
		CompactObj new_hash = CompactObj::fromHash();
		new_hash.setObj(hash_table);
		db->Set(key, std::move(new_hash));
		hash_obj = db->Find(key);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	for (size_t i = 1; i < args.size(); i += 2) {
		std::string field = args[i].toString();
		std::string value = args[i + 1].toString();
		(*hash_table)[field] = value;
	}

	return RESPParser::make_simple_string("OK");
}

std::string HashFamily::HMGet(const std::vector<CompactObj>& args) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for HMGET");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		std::string result = RESPParser::make_array(args.size() - 1);
		for (size_t i = 1; i < args.size(); i++) {
			result += RESPParser::make_null_bulk_string();
		}
		return result;
	}

	auto hash_table = hash_obj->getObj<HashType>();

	std::string result = RESPParser::make_array(args.size() - 1);
	for (size_t i = 1; i < args.size(); i++) {
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

std::string HashFamily::HDel(const std::vector<CompactObj>& args) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for HDEL");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();

	int deleted = 0;
	for (size_t i = 1; i < args.size(); i++) {
		std::string field = args[i].toString();
		if (hash_table->erase(field)) {
			deleted++;
		}
	}

	return RESPParser::make_integer(deleted);
}

std::string HashFamily::HExists(const std::vector<CompactObj>& args) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HEXISTS");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[1].toString();

	return RESPParser::make_integer(hash_table->count(field) ? 1 : 0);
}

std::string HashFamily::HLen(const std::vector<CompactObj>& args) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for HLEN");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();
	return RESPParser::make_integer(static_cast<int64_t>(hash_table->size()));
}

std::string HashFamily::HKeys(const std::vector<CompactObj>& args) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for HKEYS");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
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

std::string HashFamily::HVals(const std::vector<CompactObj>& args) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for HVALS");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
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

std::string HashFamily::HGetAll(const std::vector<CompactObj>& args) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for HGETALL");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
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

std::string HashFamily::HIncrBy(const std::vector<CompactObj>& args) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for HINCRBY");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_error("WRONGTYPE Operation against a key holding the wrong kind of value");
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[1].toString();
	std::string increment_str = args[2].toString();
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

std::string HashFamily::HScan(const std::vector<CompactObj>& args) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for HSCAN");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
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

std::string HashFamily::HStrLen(const std::vector<CompactObj>& args) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for HSTRLEN");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_integer(0);
	}

	auto hash_table = hash_obj->getObj<HashType>();
	std::string field = args[1].toString();

	auto it = hash_table->find(field);
	if (it == hash_table->end()) {
		return RESPParser::make_integer(0);
	}

	return RESPParser::make_integer(static_cast<int64_t>(it->second.length()));
}

std::string HashFamily::HRandField(const std::vector<CompactObj>& args) {
	if (args.size() < 1 || args.size() > 2) {
		return RESPParser::make_error("wrong number of arguments for HRANDFIELD");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* hash_obj = db->Find(key);

	if (hash_obj == nullptr || !hash_obj->isHash()) {
		return RESPParser::make_null_bulk_string();
	}

	auto hash_table = hash_obj->getObj<HashType>();

	if (hash_table->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	if (args.size() == 1) {
		auto it = hash_table->begin();
		size_t offset = std::rand() % hash_table->size();
		std::advance(it, offset);
		return RESPParser::make_bulk_string(it->first);
	}

	int64_t count = 1;
	if (!ParseLongLong(args[1].toString(), &count) || count < 0) {
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
