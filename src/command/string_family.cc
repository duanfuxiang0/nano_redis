#include "command/string_family.h"
#include "server/connection.h"
#include "core/database.h"
#include "core/command_context.h"
#include "core/nano_obj.h"
#include "core/util.h"
#include "server/sharding.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "protocol/resp_parser.h"
#include <photon/common/alog.h>
#include <charconv>
#include <limits>
#include <optional>
#include <system_error>
#include <unordered_map>

namespace {

constexpr const char* kInvalidIntegerError = "value is not an integer or out of range";
constexpr const char* kInvalidExpireTimeError = "invalid expire time in 'set' command";
using CommandMeta = CommandRegistry::CommandMeta;
constexpr uint32_t kReadOnly = CommandRegistry::kCmdFlagReadOnly;
constexpr uint32_t kWrite = CommandRegistry::kCmdFlagWrite;
constexpr uint32_t kAdmin = CommandRegistry::kCmdFlagAdmin;
constexpr uint32_t kMultiKey = CommandRegistry::kCmdFlagMultiKey;
constexpr uint32_t kNoKey = CommandRegistry::kCmdFlagNoKey;

} // namespace

void StringFamily::Register(CommandRegistry* registry) {
	registry->RegisterCommandWithContext(
	    "SET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Set(args, ctx); },
	    CommandMeta {-3, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "GET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Get(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kReadOnly});
	registry->RegisterCommandWithContext(
	    "DEL", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Del(args, ctx); },
	    CommandMeta {-2, 1, -1, 1, kWrite | kMultiKey});
	registry->RegisterCommandWithContext(
	    "EXISTS", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Exists(args, ctx); },
	    CommandMeta {-2, 1, -1, 1, kReadOnly | kMultiKey});
	registry->RegisterCommandWithContext(
	    "MSET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return MSet(args, ctx); },
	    CommandMeta {-3, 1, -1, 2, kWrite | kMultiKey});
	registry->RegisterCommandWithContext(
	    "MGET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return MGet(args, ctx); },
	    CommandMeta {-2, 1, -1, 1, kReadOnly | kMultiKey});
	registry->RegisterCommandWithContext(
	    "INCR", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Incr(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "DECR", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Decr(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "INCRBY", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return IncrBy(args, ctx); },
	    CommandMeta {3, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "DECRBY", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return DecrBy(args, ctx); },
	    CommandMeta {3, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "APPEND", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Append(args, ctx); },
	    CommandMeta {3, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "STRLEN", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return StrLen(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kReadOnly});
	registry->RegisterCommandWithContext(
	    "TYPE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Type(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kReadOnly});
	registry->RegisterCommandWithContext(
	    "GETRANGE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return GetRange(args, ctx); },
	    CommandMeta {4, 1, 1, 1, kReadOnly});
	registry->RegisterCommandWithContext(
	    "SETRANGE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return SetRange(args, ctx); },
	    CommandMeta {4, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "EXPIRE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Expire(args, ctx); },
	    CommandMeta {3, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "TTL", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return TTL(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kReadOnly});
	registry->RegisterCommandWithContext(
	    "PERSIST", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Persist(args, ctx); },
	    CommandMeta {2, 1, 1, 1, kWrite});
	registry->RegisterCommandWithContext(
	    "SELECT", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Select(args, ctx); },
	    CommandMeta {2, 0, 0, 0, kAdmin | kNoKey});
	registry->RegisterCommandWithContext(
	    "KEYS", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Keys(args, ctx); },
	    CommandMeta {2, 0, 0, 0, kReadOnly | kNoKey});
	registry->RegisterCommandWithContext(
	    "FLUSHDB", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return FlushDB(args, ctx); },
	    CommandMeta {1, 0, 0, 0, kWrite | kAdmin | kNoKey});
	registry->RegisterCommandWithContext(
	    "DBSIZE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return DBSize(args, ctx); },
	    CommandMeta {1, 0, 0, 0, kReadOnly | kNoKey});
	registry->RegisterCommandWithContext(
	    "PING", [](const std::vector<NanoObj>&, CommandContext*) { return RESPParser::pong_response(); },
	    CommandMeta {-1, 0, 0, 0, kReadOnly | kNoKey});
	registry->RegisterCommandWithContext(
	    "QUIT", [](const std::vector<NanoObj>&, CommandContext*) { return RESPParser::ok_response(); },
	    CommandMeta {1, 0, 0, 0, kNoKey});
	registry->RegisterCommandWithContext(
	    "HELLO", [](const std::vector<NanoObj>& args, CommandContext*) { return Hello(args); },
	    CommandMeta {-1, 0, 0, 0, kReadOnly | kNoKey});
	registry->RegisterCommandWithContext(
	    "COMMAND",
	    [registry](const std::vector<NanoObj>&, CommandContext*) { return registry->BuildCommandInfoResponse(); },
	    CommandMeta {-1, 0, 0, 0, kReadOnly | kNoKey});
}

std::string StringFamily::Set(const std::vector<NanoObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 3 && args.size() != 5) {
		return RESPParser::make_error("wrong number of arguments for 'SET'");
	}

	const NanoObj& key = args[1];
	const NanoObj& value = args[2];

	int64_t ttl_ms = -1;
	if (args.size() == 5) {
		auto ttl_value = ParseInt(args[4].ToString());
		if (!ttl_value || *ttl_value <= 0) {
			return RESPParser::make_error(kInvalidExpireTimeError);
		}

		const std::string_view option = args[3].GetStringView();
		if (EqualsIgnoreCase(option, "EX")) {
			if (*ttl_value > std::numeric_limits<int64_t>::max() / 1000) {
				ttl_ms = std::numeric_limits<int64_t>::max();
			} else {
				ttl_ms = *ttl_value * 1000;
			}
		} else if (EqualsIgnoreCase(option, "PX")) {
			ttl_ms = *ttl_value;
		} else {
			return RESPParser::make_error("syntax error");
		}
	}

	db->Set(key, NanoObj(value));
	if (ttl_ms >= 0) {
		(void)db->Expire(key, ttl_ms);
	} else {
		(void)db->Persist(key);
	}
	return RESPParser::ok_response();
}

std::string StringFamily::Get(const std::vector<NanoObj>& args, CommandContext* ctx) {
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

std::string StringFamily::Del(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'DEL'");
	}

	if (ctx->IsSingleShard() || ctx->shard_set == nullptr) {
		auto* db = ctx->GetDB();
		int count = 0;
		for (size_t i = 1; i < args.size(); ++i) {
			if (db->Del(args[i])) {
				++count;
			}
		}
		return RESPParser::make_integer(count);
	}

	struct KeyReq {
		std::string key;
	};

	std::unordered_map<size_t, std::vector<KeyReq>> shard_to_keys;
	shard_to_keys.reserve(args.size() - 1);

	for (size_t i = 1; i < args.size(); ++i) {
		std::string key = args[i].ToString();
		const size_t shard_id = Shard(key, ctx->GetShardCount());
		shard_to_keys[shard_id].push_back(KeyReq {std::move(key)});
	}

	int count = 0;

	for (auto& [shard_id, keys] : shard_to_keys) {
		int shard_deleted =
		    ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex(), keys = std::move(keys)]() mutable -> int {
			    EngineShard* shard = EngineShard::Tlocal();
			    if (shard == nullptr) {
				    return 0;
			    }
			    auto& db = shard->GetDB();
			    db.Select(db_index);
			    int local_deleted = 0;
			    for (const auto& k : keys) {
				    if (db.Del(NanoObj::FromKey(k.key))) {
					    ++local_deleted;
				    }
			    }
			    return local_deleted;
		    });
		count += shard_deleted;
	}

	return RESPParser::make_integer(count);
}

std::string StringFamily::Exists(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'EXISTS'");
	}

	if (ctx->IsSingleShard() || ctx->shard_set == nullptr) {
		auto* db = ctx->GetDB();
		int count = 0;
		for (size_t i = 1; i < args.size(); ++i) {
			if (db->Exists(args[i])) {
				++count;
			}
		}
		return RESPParser::make_integer(count);
	}

	struct KeyReq {
		std::string key;
	};

	std::unordered_map<size_t, std::vector<KeyReq>> shard_to_keys;
	shard_to_keys.reserve(args.size() - 1);

	for (size_t i = 1; i < args.size(); ++i) {
		std::string key = args[i].ToString();
		const size_t shard_id = Shard(key, ctx->GetShardCount());
		shard_to_keys[shard_id].push_back(KeyReq {std::move(key)});
	}

	int count = 0;
	for (auto& [shard_id, keys] : shard_to_keys) {
		int shard_count =
		    ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex(), keys = std::move(keys)]() mutable -> int {
			    EngineShard* shard = EngineShard::Tlocal();
			    if (shard == nullptr) {
				    return 0;
			    }
			    auto& db = shard->GetDB();
			    db.Select(db_index);
			    int local_exists = 0;
			    for (const auto& k : keys) {
				    if (db.Exists(NanoObj::FromKey(k.key))) {
					    ++local_exists;
				    }
			    }
			    return local_exists;
		    });
		count += shard_count;
	}

	return RESPParser::make_integer(count);
}

std::string StringFamily::MSet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() < 3 || (args.size() - 1) % 2 != 0) {
		return RESPParser::make_error("wrong number of arguments for 'MSET'");
	}

	size_t num_pairs = (args.size() - 1) / 2;

	if (ctx->IsSingleShard() || ctx->shard_set == nullptr) {
		auto* db = ctx->GetDB();
		for (size_t i = 1; i < args.size(); i += 2) {
			db->Set(args[i], NanoObj(args[i + 1]));
			(void)db->Persist(args[i]);
		}
		return RESPParser::ok_response();
	}

	struct KvPair {
		std::string key;
		std::string value;
	};

	std::unordered_map<size_t, std::vector<KvPair>> shard_to_pairs;
	shard_to_pairs.reserve(num_pairs);

	for (size_t i = 1; i < args.size(); i += 2) {
		std::string key = args[i].ToString();
		std::string value = args[i + 1].ToString();
		const size_t shard_id = Shard(key, ctx->GetShardCount());
		shard_to_pairs[shard_id].push_back(KvPair {std::move(key), std::move(value)});
	}

	for (auto& [shard_id, pairs] : shard_to_pairs) {
		ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex(), pairs = std::move(pairs)]() mutable {
			EngineShard* shard = EngineShard::Tlocal();
			if (shard == nullptr) {
				return;
			}
			auto& db = shard->GetDB();
			db.Select(db_index);
			for (const auto& kv : pairs) {
				NanoObj key = NanoObj::FromKey(kv.key);
				db.Set(key, NanoObj::FromKey(kv.value));
				(void)db.Persist(key);
			}
		});
	}

	return RESPParser::ok_response();
}

std::string StringFamily::MGet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for 'MGET'");
	}

	size_t num_keys = args.size() - 1;

	if (ctx->IsSingleShard() || ctx->shard_set == nullptr) {
		auto* db = ctx->GetDB();
		std::string result = RESPParser::make_array(static_cast<int64_t>(num_keys));
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

	struct KeyReq {
		size_t index;
		std::string key;
	};

	std::unordered_map<size_t, std::vector<KeyReq>> shard_to_reqs;
	shard_to_reqs.reserve(num_keys);

	for (size_t i = 1; i < args.size(); ++i) {
		std::string key = args[i].ToString();
		const size_t shard_id = Shard(key, ctx->GetShardCount());
		shard_to_reqs[shard_id].push_back(KeyReq {i - 1, std::move(key)});
	}

	std::vector<std::optional<std::string>> final_values(num_keys);

	for (auto& [shard_id, reqs] : shard_to_reqs) {
		auto results = ctx->shard_set->Await(shard_id,
		                                     [db_index = ctx->GetDBIndex(), reqs = std::move(reqs)]() mutable
		                                     -> std::vector<std::pair<size_t, std::optional<std::string>>> {
			                                     std::vector<std::pair<size_t, std::optional<std::string>>> out;
			                                     out.reserve(reqs.size());

			                                     EngineShard* shard = EngineShard::Tlocal();
			                                     if (shard == nullptr) {
				                                     return out;
			                                     }
			                                     auto& db = shard->GetDB();
			                                     db.Select(db_index);
			                                     for (const auto& req : reqs) {
				                                     out.emplace_back(req.index, db.Get(NanoObj::FromKey(req.key)));
			                                     }
			                                     return out;
		                                     });

		for (auto& [idx, val] : results) {
			final_values[idx] = std::move(val);
		}
	}

	std::string result = RESPParser::make_array(static_cast<int64_t>(num_keys));
	for (size_t i = 0; i < num_keys; ++i) {
		if (final_values[i]) {
			result += RESPParser::make_bulk_string(*final_values[i]);
		} else {
			result += RESPParser::make_null_bulk_string();
		}
	}

	return result;
}

std::string StringFamily::Incr(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'INCR'");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];

	int64_t new_value;
	auto current = db->Get(key);
	if (current) {
		auto current_value = ParseInt(*current);
		if (!current_value) {
			return RESPParser::make_error(kInvalidIntegerError);
		}
		new_value = *current_value + 1;
	} else {
		new_value = 1;
	}

	db->Set(key, NanoObj::FromInt(new_value));
	return RESPParser::make_integer(new_value);
}

std::string StringFamily::Decr(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'DECR'");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];

	int64_t new_value;
	auto current = db->Get(key);
	if (current) {
		auto current_value = ParseInt(*current);
		if (!current_value) {
			return RESPParser::make_error(kInvalidIntegerError);
		}
		new_value = *current_value - 1;
	} else {
		new_value = -1;
	}

	db->Set(key, NanoObj::FromInt(new_value));
	return RESPParser::make_integer(new_value);
}

std::string StringFamily::IncrBy(const std::vector<NanoObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'INCRBY'");
	}

	const NanoObj& key = args[1];
	auto increment = ParseInt(args[2].ToString());
	if (!increment) {
		return RESPParser::make_error(kInvalidIntegerError);
	}

	int64_t new_value;
	auto current = db->Get(key);
	if (current) {
		auto current_value = ParseInt(*current);
		if (!current_value) {
			return RESPParser::make_error(kInvalidIntegerError);
		}
		new_value = *current_value + *increment;
	} else {
		new_value = *increment;
	}

	db->Set(key, NanoObj::FromInt(new_value));
	return RESPParser::make_integer(new_value);
}

std::string StringFamily::DecrBy(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'DECRBY'");
	}

	auto* db = ctx->GetDB();
	auto decrement = ParseInt(args[2].ToString());
	if (!decrement) {
		return RESPParser::make_error(kInvalidIntegerError);
	}

	const NanoObj& key = args[1];
	int64_t new_value;
	auto current = db->Get(key);
	if (current) {
		auto current_value = ParseInt(*current);
		if (!current_value) {
			return RESPParser::make_error(kInvalidIntegerError);
		}
		new_value = *current_value - *decrement;
	} else {
		new_value = -*decrement;
	}

	db->Set(key, NanoObj::FromInt(new_value));
	return RESPParser::make_integer(new_value);
}

std::string StringFamily::Append(const std::vector<NanoObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'APPEND'");
	}

	const NanoObj& key = args[1];
	const NanoObj& value = args[2];

	std::string new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = *current + value.ToString();
	} else {
		new_value = value.ToString();
	}

	db->Set(key, NanoObj::FromKey(new_value));
	return RESPParser::make_integer(static_cast<int64_t>(new_value.length()));
}

std::string StringFamily::StrLen(const std::vector<NanoObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'STRLEN'");
	}

	auto val = db->Get(args[1]);
	if (val) {
		return RESPParser::make_integer(static_cast<int64_t>(val->length()));
	} else {
		return RESPParser::make_integer(0);
	}
}

std::string StringFamily::Type(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'TYPE'");
	}

	auto* db = ctx->GetDB();
	const NanoObj* value = db->Find(args[1]);
	if (value == nullptr) {
		return RESPParser::make_simple_string("none");
	}

	switch (value->GetType()) {
	case OBJ_STRING:
		return RESPParser::make_simple_string("string");
	case OBJ_HASH:
		return RESPParser::make_simple_string("hash");
	case OBJ_SET:
		return RESPParser::make_simple_string("set");
	case OBJ_LIST:
		return RESPParser::make_simple_string("list");
	case OBJ_ZSET:
		return RESPParser::make_simple_string("zset");
	default:
		return RESPParser::make_simple_string("none");
	}
}

std::string StringFamily::GetRange(const std::vector<NanoObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for 'GETRANGE'");
	}

	const NanoObj& key = args[1];
	auto start = ParseInt(args[2].ToString());
	auto end = ParseInt(args[3].ToString());
	if (!start || !end) {
		return RESPParser::make_error(kInvalidIntegerError);
	}

	auto val = db->Get(key);
	if (!val) {
		return RESPParser::make_bulk_string("");
	}

	std::string s = *val;
	int64_t len = static_cast<int64_t>(s.length());

	int64_t adjusted_start = AdjustIndex(*start, len);
	int64_t adjusted_end = AdjustIndex(*end, len);

	if (adjusted_start > adjusted_end) {
		return RESPParser::make_bulk_string("");
	}

	std::string result =
	    s.substr(static_cast<size_t>(adjusted_start), static_cast<size_t>(adjusted_end - adjusted_start + 1));
	return RESPParser::make_bulk_string(result);
}

std::string StringFamily::SetRange(const std::vector<NanoObj>& args, CommandContext* ctx) {
	auto* db = ctx->GetDB();

	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for 'SETRANGE'");
	}

	const NanoObj& key = args[1];
	auto offset = ParseInt(args[2].ToString());
	if (!offset) {
		return RESPParser::make_error(kInvalidIntegerError);
	}
	const NanoObj& value = args[3];

	std::string new_value;
	auto current = db->Get(key);
	if (current) {
		new_value = *current;
	}

	if (*offset < 0) {
		*offset = 0;
	}

	std::string value_str = value.ToString();
	size_t offset_sz = static_cast<size_t>(*offset);
	size_t new_length = std::max(new_value.length(), offset_sz + value_str.length());
	new_value.resize(new_length, '\0');

	for (size_t i = 0; i < value_str.length(); ++i) {
		new_value[offset_sz + i] = value_str[i];
	}

	db->Set(key, NanoObj::FromKey(new_value));
	return RESPParser::make_integer(static_cast<int64_t>(new_value.length()));
}

std::string StringFamily::Expire(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for 'EXPIRE'");
	}

	auto* db = ctx->GetDB();
	auto seconds = ParseInt(args[2].ToString());
	if (!seconds) {
		return RESPParser::make_error(kInvalidIntegerError);
	}

	int64_t ttl_ms = 0;
	if (*seconds > 0) {
		if (*seconds > std::numeric_limits<int64_t>::max() / 1000) {
			ttl_ms = std::numeric_limits<int64_t>::max();
		} else {
			ttl_ms = *seconds * 1000;
		}
	}

	return RESPParser::make_integer(db->Expire(args[1], ttl_ms) ? 1 : 0);
}

std::string StringFamily::TTL(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'TTL'");
	}

	auto* db = ctx->GetDB();
	return RESPParser::make_integer(db->TTL(args[1]));
}

std::string StringFamily::Persist(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'PERSIST'");
	}

	auto* db = ctx->GetDB();
	return RESPParser::make_integer(db->Persist(args[1]) ? 1 : 0);
}

std::string StringFamily::Select(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for 'SELECT'");
	}

	auto db_index = ParseInt(args[1].ToString());
	if (!db_index) {
		return RESPParser::make_error(kInvalidIntegerError);
	}
	if (*db_index < 0) {
		return RESPParser::make_error("DB index out of range");
	}
	size_t db_index_size_t = static_cast<size_t>(*db_index);
	if (db_index_size_t >= Database::kNumDBs) {
		return RESPParser::make_error("DB index out of range");
	}

	if (ctx->connection != nullptr) {
		if (!ctx->connection->SetDBIndex(db_index_size_t)) {
			return RESPParser::make_error("DB index out of range");
		}
		ctx->db_index = db_index_size_t;
		return RESPParser::ok_response();
	}

	auto* db = ctx->GetDB();

	// In sharded mode we keep database selection consistent across all shards
	// (current implementation is global, not per-connection).
	if (ctx->shard_set && !ctx->IsSingleShard()) {
		for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
			bool ok = ctx->shard_set->Await(shard_id, [db_index_size_t]() -> bool {
				EngineShard* shard = EngineShard::Tlocal();
				if (shard == nullptr) {
					return false;
				}
				auto& shard_db = shard->GetDB();
				return shard_db.Select(db_index_size_t);
			});

			if (!ok) {
				return RESPParser::make_error("DB index out of range");
			}
		}

		ctx->db_index = db_index_size_t;
		return RESPParser::ok_response();
	}

	if (!db->Select(db_index_size_t)) {
		return RESPParser::make_error("DB index out of range");
	}

	ctx->db_index = db_index_size_t;
	return RESPParser::ok_response();
}

std::string StringFamily::Keys(const std::vector<NanoObj>& args, CommandContext* ctx) {
	(void)args;
	if (!ctx->shard_set || ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		std::vector<std::string> keys = db->Keys();
		std::string response = RESPParser::make_array(static_cast<int64_t>(keys.size()));
		for (const auto& key : keys) {
			response += RESPParser::make_bulk_string(key);
		}
		return response;
	}

	std::vector<std::string> all_keys;
	for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
		auto shard_keys = ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex()]() -> std::vector<std::string> {
			EngineShard* shard = EngineShard::Tlocal();
			if (!shard) {
				return {};
			}
			auto& db = shard->GetDB();
			db.Select(db_index);
			return db.Keys();
		});
		all_keys.insert(all_keys.end(), shard_keys.begin(), shard_keys.end());
	}

	std::string response = RESPParser::make_array(static_cast<int64_t>(all_keys.size()));
	for (const auto& key : all_keys) {
		response += RESPParser::make_bulk_string(key);
	}
	return response;
}

std::string StringFamily::FlushDB(const std::vector<NanoObj>& args, CommandContext* ctx) {
	(void)args;
	if (!ctx->shard_set || ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		db->ClearCurrentDB();
		return RESPParser::ok_response();
	}

	for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
		ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex()]() {
			EngineShard* shard = EngineShard::Tlocal();
			if (shard) {
				auto& db = shard->GetDB();
				db.Select(db_index);
				db.ClearCurrentDB();
			}
		});
	}

	return RESPParser::ok_response();
}

std::string StringFamily::DBSize(const std::vector<NanoObj>& args, CommandContext* ctx) {
	(void)args;
	if (!ctx->shard_set || ctx->IsSingleShard()) {
		auto* db = ctx->GetDB();
		size_t count = db->KeyCount();
		return RESPParser::make_integer(static_cast<int64_t>(count));
	}

	size_t total_count = 0;
	for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
		size_t shard_count = ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex()]() -> size_t {
			EngineShard* shard = EngineShard::Tlocal();
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

std::optional<int64_t> StringFamily::ParseInt(const std::string& s) {
	int64_t value = 0;
	const char* start = s.data();
	const char* end = s.data() + s.size();
	auto [ptr, ec] = std::from_chars(start, end, value, 10);
	if (ec != std::errc() || ptr != end) {
		return std::nullopt;
	}
	return value;
}

int64_t StringFamily::AdjustIndex(int64_t index, int64_t length) {
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

std::string StringFamily::Hello(const std::vector<NanoObj>& args) {
	// HELLO [protover [AUTH username password] [SETNAME clientname]]
	// Returns server information as a map (array of key-value pairs in RESP2)
	int64_t proto_ver = 2; // Default to RESP2
	if (args.size() > 1) {
		auto parsed_proto_ver = ParseInt(args[1].ToString());
		if (!parsed_proto_ver) {
			return RESPParser::make_error("NOPROTO unsupported protocol version");
		}
		proto_ver = *parsed_proto_ver;
		if (proto_ver < 2 || proto_ver > 3) {
			return RESPParser::make_error("NOPROTO unsupported protocol version");
		}
	}

	// For now, we only support RESP2 (protocol version 2)
	// Return a map as an array: [key1, val1, key2, val2, ...]
	std::string response = RESPParser::make_array(14); // 7 key-value pairs
	response += RESPParser::make_bulk_string("server");
	response += RESPParser::make_bulk_string("nano_redis");
	response += RESPParser::make_bulk_string("version");
	response += RESPParser::make_bulk_string("0.1.0");
	response += RESPParser::make_bulk_string("proto");
	response += RESPParser::make_integer(2); // We only support RESP2
	response += RESPParser::make_bulk_string("id");
	response += RESPParser::make_integer(1);
	response += RESPParser::make_bulk_string("mode");
	response += RESPParser::make_bulk_string("standalone");
	response += RESPParser::make_bulk_string("role");
	response += RESPParser::make_bulk_string("master");
	response += RESPParser::make_bulk_string("modules");
	response += RESPParser::make_array(0); // No modules

	return response;
}
