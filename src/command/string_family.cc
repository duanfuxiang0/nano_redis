#include "command/string_family.h"
#include "server/connection.h"
#include "storage/database.h"
#include "protocol/resp_parser.h"
#include <photon/common/alog.h>
#include <iostream>
#include <cstdlib>

namespace {
Database* g_database = nullptr;

Database* GetDatabase() {
	if (g_database) {
		return g_database;
	}
	static thread_local Database database;
	return &database;
}
} // namespace

void StringFamily::SetDatabase(Database* db) {
	g_database = db;
}

void StringFamily::Register(CommandRegistry* registry) {
	registry->register_command("SET", [](const std::vector<std::string>& args) { return Set(args); });
	registry->register_command("GET", [](const std::vector<std::string>& args) { return Get(args); });
	registry->register_command("DEL", [](const std::vector<std::string>& args) { return Del(args); });
	registry->register_command("EXISTS", [](const std::vector<std::string>& args) { return Exists(args); });
	registry->register_command("MSET", [](const std::vector<std::string>& args) { return MSet(args); });
	registry->register_command("MGET", [](const std::vector<std::string>& args) { return MGet(args); });
	registry->register_command("INCR", [](const std::vector<std::string>& args) { return Incr(args); });
	registry->register_command("DECR", [](const std::vector<std::string>& args) { return Decr(args); });
	registry->register_command("INCRBY", [](const std::vector<std::string>& args) { return IncrBy(args); });
	registry->register_command("DECRBY", [](const std::vector<std::string>& args) { return DecrBy(args); });
	registry->register_command("APPEND", [](const std::vector<std::string>& args) { return Append(args); });
	registry->register_command("STRLEN", [](const std::vector<std::string>& args) { return StrLen(args); });
	registry->register_command("GETRANGE", [](const std::vector<std::string>& args) { return GetRange(args); });
	registry->register_command("SETRANGE", [](const std::vector<std::string>& args) { return SetRange(args); });
	registry->register_command("SELECT", [](const std::vector<std::string>& args) { return Select(args); });
	registry->register_command("KEYS", [](const std::vector<std::string>& args) { return Keys(args); });
	registry->register_command(
	    "PING", [](const std::vector<std::string>& args) { return RESPParser::make_simple_string("PONG"); });
	registry->register_command(
	    "QUIT", [](const std::vector<std::string>& args) { return RESPParser::make_simple_string("OK"); });
}

std::string StringFamily::Set(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'SET'");
	}

	const std::string& key = args[1];
	const std::string& value = args[2];

	db->Set(key, value);
	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::Get(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'GET'");
	}

	auto result = db->Get(args[1]);
	if (result) {
		return RESPParser::make_bulk_string(*result);
	} else {
		return RESPParser::make_null_bulk_string();
	}
}

std::string StringFamily::Del(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'DEL'");
	}

	int count = 0;
	for (size_t i = 1; i < args.size(); ++i) {
		if (db->Del(args[i])) {
			++count;
		}
	}

	return RESPParser::make_integer(count);
}

std::string StringFamily::Exists(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'EXISTS'");
	}

	int count = 0;
	for (size_t i = 1; i < args.size(); ++i) {
		if (db->Exists(args[i])) {
			++count;
		}
	}

	return RESPParser::make_integer(count);
}

std::string StringFamily::MSet(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() < 3 || (args.size() - 1) % 2 != 0) {
		return RESPParser::make_error("wrong number of arguments for 'MSET'");
	}

	for (size_t i = 1; i < args.size(); i += 2) {
		db->Set(args[i], args[i + 1]);
	}

	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::MGet(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'MGET'");
	}

	std::string result = RESPParser::make_array(args.size() - 1);
	for (size_t i = 1; i < args.size(); ++i) {
		auto val = db->Get(args[i]);
		if (val) {
			result += RESPParser::make_bulk_string(*val);
		} else {
			result += RESPParser::make_null_bulk_string();
		}
	}

	return result;
}

std::string StringFamily::Incr(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'INCR'");
	}

	std::vector<std::string> incr_args = {"INCRBY", args[1], "1"};
	return IncrBy(incr_args);
}

std::string StringFamily::Decr(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'DECR'");
	}

	std::vector<std::string> incr_args = {"DECRBY", args[1], "-1"};
	return IncrBy(incr_args);
}

std::string StringFamily::IncrBy(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'INCRBY'");
	}

	const std::string& key = args[1];
	int64_t increment = ParseInt(args[2]);

	int64_t new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = ParseInt(*current) + increment;
	} else {
		new_value = increment;
	}

	db->Set(key, IntToString(new_value));
	return RESPParser::make_integer(new_value);
}

std::string StringFamily::DecrBy(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'DECRBY'");
	}

	std::vector<std::string> incr_args = {"INCRBY", args[1], "-" + args[2]};
	return IncrBy(incr_args);
}

std::string StringFamily::Append(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'APPEND'");
	}

	const std::string& key = args[1];
	const std::string& value = args[2];

	std::string new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = *current + value;
	} else {
		new_value = value;
	}

	db->Set(key, new_value);
	return RESPParser::make_integer(new_value.length());
}

std::string StringFamily::StrLen(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'STRLEN'");
	}

	auto val = db->Get(args[1]);
	if (val) {
		return RESPParser::make_integer(val->length());
	} else {
		return RESPParser::make_integer(0);
	}
}

std::string StringFamily::GetRange(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for 'GETRANGE'");
	}

	const std::string& key = args[1];
	int start = ParseInt(args[2]);
	int end = ParseInt(args[3]);

	auto val = db->Get(key);
	if (!val) {
		return RESPParser::make_bulk_string("");
	}

	std::string s = *val;
	int len = s.length();

	start = AdjustIndex(start, len);
	end = AdjustIndex(end, len);

	if (start > end) {
		return RESPParser::make_bulk_string("");
	}

	std::string result = s.substr(start, end - start + 1);
	return RESPParser::make_bulk_string(result);
}

std::string StringFamily::SetRange(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for 'SETRANGE'");
	}

	const std::string& key = args[1];
	int offset = ParseInt(args[2]);
	const std::string& value = args[3];

	std::string new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = *current;
	}

	if (offset < 0) {
		offset = 0;
	}

	size_t new_length = std::max(new_value.length(), static_cast<size_t>(offset) + value.length());
	new_value.resize(new_length, '\0');

	for (size_t i = 0; i < value.length(); ++i) {
		new_value[offset + i] = value[i];
	}

	db->Set(key, new_value);
	return RESPParser::make_integer(new_value.length());
}

std::string StringFamily::Select(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'SELECT'");
	}

	size_t db_index = static_cast<size_t>(ParseInt(args[1]));
	if (!db->Select(db_index)) {
		return RESPParser::make_error("DB index out of range");
	}

	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::Keys(const std::vector<std::string>& args) {
	auto* db = GetDatabase();

	std::vector<std::string> keys = db->Keys();
	std::string response = RESPParser::make_array(keys.size());
	for (const auto& key : keys) {
		response += RESPParser::make_bulk_string(key);
	}
	return response;
}

void StringFamily::ClearDatabase() {
	auto* db = GetDatabase();
	db->ClearAll();
}

int64_t StringFamily::ParseInt(const std::string& s) {
	char* end;
	return std::strtoll(s.c_str(), &end, 10);
}

std::string StringFamily::IntToString(int64_t value) {
	return std::to_string(value);
}

int StringFamily::AdjustIndex(int index, int length) {
	if (index < 0) {
		index += length;
	}
	if (index < 0) {
		index = 0;
	}
	if (index >= length) {
		index = length - 1;
	}
	return index;
}
