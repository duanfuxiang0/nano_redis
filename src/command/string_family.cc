#include "command/string_family.h"
#include "server/connection.h"
#include "core/database.h"
#include "core/command_context.h"
#include "core/compact_obj.h"
#include "server/sharding.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "protocol/resp_parser.h"
#include <photon/common/alog.h>
#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <optional>

void StringFamily::Register(CommandRegistry* registry) {
	registry->register_command_with_context("SET", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Set(args, ctx); });
	registry->register_command_with_context("GET", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Get(args, ctx); });
	registry->register_command_with_context("DEL", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Del(args, ctx); });
	registry->register_command_with_context("EXISTS", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Exists(args, ctx); });
	registry->register_command_with_context("MSET", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return MSet(args, ctx); });
	registry->register_command_with_context("MGET", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return MGet(args, ctx); });
	registry->register_command_with_context("INCR", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Incr(args, ctx); });
	registry->register_command_with_context("DECR", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Decr(args, ctx); });
	registry->register_command_with_context("INCRBY", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return IncrBy(args, ctx); });
	registry->register_command_with_context("DECRBY", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return DecrBy(args, ctx); });
	registry->register_command_with_context("APPEND", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Append(args, ctx); });
	registry->register_command_with_context("STRLEN", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return StrLen(args, ctx); });
	registry->register_command_with_context("GETRANGE", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return GetRange(args, ctx); });
	registry->register_command_with_context("SETRANGE", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SetRange(args, ctx); });
	registry->register_command_with_context("SELECT", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Select(args, ctx); });
	registry->register_command_with_context("KEYS", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return Keys(args, ctx); });
	registry->register_command_with_context("FLUSHDB", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return FlushDB(args, ctx); });
	registry->register_command_with_context("DBSIZE", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return DBSize(args, ctx); });
	registry->register_command_with_context(
	    "PING", [](const std::vector<CompactObj>&, CommandContext*) { return RESPParser::make_simple_string("PONG"); });
	registry->register_command_with_context(
	    "QUIT", [](const std::vector<CompactObj>&, CommandContext*) { return RESPParser::make_simple_string("OK"); });
	registry->register_command_with_context(
	    "HELLO", [](const std::vector<CompactObj>& args, CommandContext*) { return Hello(args); });
	registry->register_command_with_context(
	    "COMMAND", [](const std::vector<CompactObj>&, CommandContext*) {
	        // Return empty array for COMMAND (used by redis-cli for command discovery)
	        return RESPParser::make_array(0);
	    });
}

std::string StringFamily::Set(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'SET'");
	}

	const CompactObj& key = args[1];
	const CompactObj& value = args[2];

	db->Set(key, CompactObj(value));
	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::Get(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

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

std::string StringFamily::Del(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

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

std::string StringFamily::Exists(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

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

std::string StringFamily::MSet(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 3 || (args.size() - 1) % 2 != 0) {
		return RESPParser::make_error("wrong number of arguments for 'MSET'");
	}

	size_t num_pairs = (args.size() - 1) / 2;

	if (ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		for (size_t i = 1; i < args.size(); i += 2) {
			db->Set(args[i], CompactObj(args[i + 1]));
		}
		return RESPParser::make_simple_string("OK");
	}

	std::unordered_map<size_t, std::vector<std::pair<CompactObj, CompactObj>>> shard_to_pairs;

	for (size_t i = 1; i < args.size(); i += 2) {
		std::string key = args[i].toString();
		size_t shard_id = Shard(key, ctx->GetShardCount());

		shard_to_pairs[shard_id].emplace_back(args[i], args[i + 1]);
	}

	for (const auto& [shard_id, pairs] : shard_to_pairs) {
		for (const auto& [key, value] : pairs) {
			ctx->GetShardDB(shard_id)->Set(key, CompactObj(value));
		}
	}

	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::MGet(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'MGET'");
	}

	size_t num_keys = args.size() - 1;

	if (ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		std::string result = RESPParser::make_array(num_keys);
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

	std::unordered_map<size_t, std::vector<size_t>> shard_to_indices;
	std::unordered_map<size_t, std::vector<CompactObj>> shard_to_keys;

	for (size_t i = 1; i < args.size(); ++i) {
		std::string key = args[i].toString();
		size_t shard_id = Shard(key, ctx->GetShardCount());

		shard_to_indices[shard_id].push_back(i - 1);
		shard_to_keys[shard_id].push_back(args[i]);
	}

	std::vector<std::optional<std::string>> final_values(num_keys);

	for (const auto& [shard_id, indices] : shard_to_indices) {
		const auto& keys = shard_to_keys[shard_id];

		for (size_t i = 0; i < indices.size(); ++i) {
			size_t original_index = indices[i];
			final_values[original_index] = ctx->GetShardDB(shard_id)->Get(keys[i]);
		}
	}

	std::string result = RESPParser::make_array(num_keys);
	for (size_t i = 0; i < num_keys; ++i) {
		if (final_values[i]) {
			result += RESPParser::make_bulk_string(*final_values[i]);
		} else {
			result += RESPParser::make_null_bulk_string();
		}
	}

	return result;
}

std::string StringFamily::Incr(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'INCR'");
	}

	std::vector<CompactObj> incr_args = {args[0], args[1], CompactObj::fromInt(1)};
	return IncrBy(incr_args, ctx);
}

std::string StringFamily::Decr(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'DECR'");
	}

	std::string neg_one = "-1";
	std::vector<CompactObj> incr_args = {args[0], args[1], CompactObj::fromKey(neg_one)};
	return IncrBy(incr_args, ctx);
}

std::string StringFamily::IncrBy(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'INCRBY'");
	}

	const CompactObj& key = args[1];
	int64_t increment = ParseInt(args[2].toString());

	int64_t new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = ParseInt(*current) + increment;
	} else {
		new_value = increment;
	}

	db->Set(key, CompactObj::fromInt(new_value));
	return RESPParser::make_integer(new_value);
}

std::string StringFamily::DecrBy(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'DECRBY'");
	}

	std::string neg_val = "-" + args[2].toString();
	std::vector<CompactObj> incr_args = {args[0], args[1], CompactObj::fromKey(neg_val)};
	return IncrBy(incr_args, ctx);
}

std::string StringFamily::Append(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'APPEND'");
	}

	const CompactObj& key = args[1];
	const CompactObj& value = args[2];

	std::string new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = *current + value.toString();
	} else {
		new_value = value.toString();
	}

	db->Set(key, CompactObj::fromKey(new_value));
	return RESPParser::make_integer(new_value.length());
}

std::string StringFamily::StrLen(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

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

std::string StringFamily::GetRange(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for 'GETRANGE'");
	}

	const CompactObj& key = args[1];
	int start = ParseInt(args[2].toString());
	int end = ParseInt(args[3].toString());

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

std::string StringFamily::SetRange(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for 'SETRANGE'");
	}

	const CompactObj& key = args[1];
	int offset = ParseInt(args[2].toString());
	const CompactObj& value = args[3];

	std::string new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = *current;
	}

	if (offset < 0) {
		offset = 0;
	}

	size_t new_length = std::max(new_value.length(), static_cast<size_t>(offset) + value.toString().length());
	new_value.resize(new_length, '\0');

	for (size_t i = 0; i < value.toString().length(); ++i) {
		new_value[offset + i] = value.toString()[i];
	}

	db->Set(key, CompactObj::fromKey(new_value));
	return RESPParser::make_integer(new_value.length());
}

std::string StringFamily::Select(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'SELECT'");
	}

	size_t db_index = static_cast<size_t>(ParseInt(args[1].toString()));
	if (!db->Select(db_index)) {
		return RESPParser::make_error("DB index out of range");
	}

	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::Keys(const std::vector<CompactObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	std::vector<std::string> keys = db->Keys();
	std::string response = RESPParser::make_array(keys.size());
	for (const auto& key : keys) {
		response += RESPParser::make_bulk_string(key);
	}
	return response;
}

std::string StringFamily::FlushDB(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (!ctx->shard_set || ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		db->ClearCurrentDB();
		return RESPParser::make_simple_string("OK");
	}

	for (size_t shard_id = 0; shard_id < ctx->shard_set->size(); ++shard_id) {
		ctx->shard_set->Add(shard_id, [db_index = ctx->GetDBIndex()]() {
			EngineShard* shard = EngineShard::tlocal();
			if (shard) {
				auto& db = shard->GetDB();
				db.Select(db_index);
				db.ClearCurrentDB();
			}
		});
	}

	return RESPParser::make_simple_string("OK");
}

std::string StringFamily::DBSize(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (!ctx->shard_set || ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		size_t count = db->KeyCount();
		return RESPParser::make_integer(static_cast<int64_t>(count));
	}

	size_t total_count = 0;
	for (size_t shard_id = 0; shard_id < ctx->shard_set->size(); ++shard_id) {
		size_t shard_count = ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex()]() -> size_t {
			EngineShard* shard = EngineShard::tlocal();
			if (shard) {
				auto& db = shard->GetDB();
				db.Select(db_index);
				return db.KeyCount();
			}
			return 0;
		});
		total_count += shard_count;
	}

	return RESPParser::make_integer(static_cast<int64_t>(total_count));
}

void StringFamily::ClearDatabase(CommandContext* ctx) {
	auto* db = ctx->GetDB();
	db->ClearAll();
}

int64_t StringFamily::ParseInt(const std::string& s) {
	char* end;
	return std::strtoll(s.c_str(), &end, 10);
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

std::string StringFamily::Hello(const std::vector<CompactObj>& args) {
	// HELLO [protover [AUTH username password] [SETNAME clientname]]
	// Returns server information as a map (array of key-value pairs in RESP2)
	int proto_ver = 2;  // Default to RESP2
	if (args.size() > 1) {
		proto_ver = ParseInt(args[1].toString());
		if (proto_ver < 2 || proto_ver > 3) {
			return RESPParser::make_error("NOPROTO unsupported protocol version");
		}
	}

	// For now, we only support RESP2 (protocol version 2)
	// Return a map as an array: [key1, val1, key2, val2, ...]
	std::string response = RESPParser::make_array(14);  // 7 key-value pairs
	response += RESPParser::make_bulk_string("server");
	response += RESPParser::make_bulk_string("nano_redis");
	response += RESPParser::make_bulk_string("version");
	response += RESPParser::make_bulk_string("0.1.0");
	response += RESPParser::make_bulk_string("proto");
	response += RESPParser::make_integer(2);  // We only support RESP2
	response += RESPParser::make_bulk_string("id");
	response += RESPParser::make_integer(1);
	response += RESPParser::make_bulk_string("mode");
	response += RESPParser::make_bulk_string("standalone");
	response += RESPParser::make_bulk_string("role");
	response += RESPParser::make_bulk_string("master");
	response += RESPParser::make_bulk_string("modules");
	response += RESPParser::make_array(0);  // No modules

	return response;
}
