#include "command/list_family.h"
#include "core/command_context.h"
#include "protocol/resp_parser.h"
#include <cstdlib>
#include <sstream>
#include <vector>
#include <algorithm>

void ListFamily::Register(CommandRegistry* registry) {
	registry->RegisterCommandWithContext(
	    "LPUSH", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LPush(args, ctx); });
	registry->RegisterCommandWithContext(
	    "RPUSH", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return RPush(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LPOP", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LPop(args, ctx); });
	registry->RegisterCommandWithContext(
	    "RPOP", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return RPop(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LLEN", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LLen(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LINDEX", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LIndex(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LSET", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LSet(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LRANGE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LRange(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LTRIM", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LTrim(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LREM", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LRem(args, ctx); });
	registry->RegisterCommandWithContext(
	    "LINSERT", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return LInsert(args, ctx); });
}

std::string ListFamily::LPush(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LPUSH key value [value ...]
	if (args.size() < 3) {
		return RESPParser::make_error("wrong number of arguments for LPUSH");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		if (list_obj != nullptr && !list_obj->isList()) {
			db->Del(key);
		}
		auto list = new std::deque<NanoObj>();
		NanoObj new_list = NanoObj::fromList();
		new_list.setObj(list);
		db->Set(key, std::move(new_list));
		list_obj = db->Find(key);
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	for (size_t i = 2; i < args.size(); i++) {
		list->push_front(args[i]);
	}

	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

std::string ListFamily::RPush(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// RPUSH key value [value ...]
	if (args.size() < 3) {
		return RESPParser::make_error("wrong number of arguments for RPUSH");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		if (list_obj != nullptr && !list_obj->isList()) {
			db->Del(key);
		}
		auto list = new std::deque<NanoObj>();
		NanoObj new_list = NanoObj::fromList();
		new_list.setObj(list);
		db->Set(key, std::move(new_list));
		list_obj = db->Find(key);
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	for (size_t i = 2; i < args.size(); i++) {
		list->push_back(args[i]);
	}

	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

std::string ListFamily::LPop(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LPOP key [count]
	if (args.size() < 2 || args.size() > 3) {
		return RESPParser::make_error("wrong number of arguments for LPOP");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_null_bulk_string();
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	if (list->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t count = 1;
	if (args.size() == 3) {
		if (!ParseLongLong(args[2].toString(), &count) || count < 0) {
			return RESPParser::make_error("count is not a valid positive integer");
		}
	}

	if (count == 1) {
		std::string result = list->front().toString();
		list->pop_front();
		if (list->empty()) {
			db->Del(key);
		}
		return RESPParser::make_bulk_string(result);
	}

	std::string result = RESPParser::make_array(count);
	for (int64_t i = 0; i < count && !list->empty(); i++) {
		result += RESPParser::make_bulk_string(list->front().toString());
		list->pop_front();
	}

	if (list->empty()) {
		db->Del(key);
	}

	return result;
}

std::string ListFamily::RPop(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// RPOP key [count]
	if (args.size() < 2 || args.size() > 3) {
		return RESPParser::make_error("wrong number of arguments for RPOP");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_null_bulk_string();
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	if (list->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t count = 1;
	if (args.size() == 3) {
		if (!ParseLongLong(args[2].toString(), &count) || count < 0) {
			return RESPParser::make_error("count is not a valid positive integer");
		}
	}

	if (count == 1) {
		std::string result = list->back().toString();
		list->pop_back();
		if (list->empty()) {
			db->Del(key);
		}
		return RESPParser::make_bulk_string(result);
	}

	std::string result = RESPParser::make_array(count);
	for (int64_t i = 0; i < count && !list->empty(); i++) {
		result += RESPParser::make_bulk_string(list->back().toString());
		list->pop_back();
	}

	if (list->empty()) {
		db->Del(key);
	}

	return result;
}

std::string ListFamily::LLen(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LLEN key
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for LLEN");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_integer(0);
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();
	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

std::string ListFamily::LIndex(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LINDEX key index
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for LINDEX");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t index;
	if (!ParseLongLong(args[2].toString(), &index)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	if (index < 0) {
		index += static_cast<int64_t>(list->size());
	}

	if (index < 0 || index >= static_cast<int64_t>(list->size())) {
		return RESPParser::make_null_bulk_string();
	}

	return RESPParser::make_bulk_string(list->at(index).toString());
}

std::string ListFamily::LSet(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LSET key index value
	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for LSET");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_error("no such key");
	}

	int64_t index;
	if (!ParseLongLong(args[2].toString(), &index)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	if (index < 0) {
		index += static_cast<int64_t>(list->size());
	}

	if (index < 0 || index >= static_cast<int64_t>(list->size())) {
		return RESPParser::make_error("index out of range");
	}

	list->at(index) = args[3];
	return RESPParser::ok_response();
}

std::string ListFamily::LRange(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LRANGE key start stop
	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for LRANGE");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_array(0);
	}

	int64_t start, stop;
	if (!ParseLongLong(args[2].toString(), &start) || !ParseLongLong(args[3].toString(), &stop)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();
	int64_t len = static_cast<int64_t>(list->size());

	if (start < 0) {
		start += len;
		if (start < 0) {
			start = 0;
		}
	}

	if (stop < 0) {
		stop += len;
		if (stop < 0) {
			stop = -1;
		}
	}

	if (start >= len) {
		return RESPParser::make_array(0);
	}

	if (stop >= len) {
		stop = len - 1;
	}

	if (start > stop) {
		return RESPParser::make_array(0);
	}

	std::string result = RESPParser::make_array(stop - start + 1);
	for (int64_t i = start; i <= stop; i++) {
		result += RESPParser::make_bulk_string(list->at(i).toString());
	}

	return result;
}

std::string ListFamily::LTrim(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LTRIM key start stop
	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for LTRIM");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::ok_response();
	}

	int64_t start, stop;
	if (!ParseLongLong(args[2].toString(), &start) || !ParseLongLong(args[3].toString(), &stop)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();
	int64_t len = static_cast<int64_t>(list->size());

	if (start < 0) {
		start += len;
		if (start < 0) {
			start = 0;
		}
	}

	if (stop < 0) {
		stop += len;
		if (stop < 0) {
			stop = -1;
		}
	}

	if (start >= len || start > stop) {
		db->Del(key);
		return RESPParser::ok_response();
	}

	if (stop >= len) {
		stop = len - 1;
	}

	std::deque<NanoObj> trimmed;
	for (int64_t i = start; i <= stop; i++) {
		trimmed.push_back(list->at(i));
	}
	*list = std::move(trimmed);

	return RESPParser::ok_response();
}

std::string ListFamily::LRem(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LREM key count value
	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for LREM");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_integer(0);
	}

	int64_t count;
	if (!ParseLongLong(args[2].toString(), &count)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<NanoObj>>();
	std::string value = args[3].toString();

	int removed = 0;
	if (count == 0) {
		auto it = list->begin();
		while (it != list->end()) {
			if (it->toString() == value) {
				it = list->erase(it);
				removed++;
			} else {
				++it;
			}
		}
	} else if (count > 0) {
		auto it = list->begin();
		while (it != list->end() && removed < count) {
			if (it->toString() == value) {
				it = list->erase(it);
				removed++;
			} else {
				++it;
			}
		}
	} else {
		auto it = list->end();
		while (it != list->begin() && removed < -count) {
			--it;
			if (it->toString() == value) {
				it = list->erase(it);
				removed++;
			}
		}
	}

	if (list->empty()) {
		db->Del(key);
	}

	return RESPParser::make_integer(removed);
}

std::string ListFamily::LInsert(const std::vector<NanoObj>& args, CommandContext* ctx) {
	// LINSERT key BEFORE|AFTER pivot value
	if (args.size() != 5) {
		return RESPParser::make_error("wrong number of arguments for LINSERT");
	}

	auto* db = ctx->GetDB();
	const NanoObj& key = args[1];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_integer(0);
	}

	std::string where = args[2].toString();
	if (where != "BEFORE" && where != "AFTER") {
		return RESPParser::make_error("syntax error");
	}

	std::string pivot = args[3].toString();
	std::string value = args[4].toString();

	auto list = list_obj->getObj<std::deque<NanoObj>>();

	auto it =
	    std::find_if(list->begin(), list->end(), [&pivot](const NanoObj& obj) { return obj.toString() == pivot; });

	if (it == list->end()) {
		return RESPParser::make_integer(-1);
	}

	if (where == "BEFORE") {
		list->insert(it, NanoObj::fromKey(value));
	} else {
		list->insert(it + 1, NanoObj::fromKey(value));
	}

	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

bool ListFamily::ParseLongLong(const std::string& s, int64_t* out) {
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
