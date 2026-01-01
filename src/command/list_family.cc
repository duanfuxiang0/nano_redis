#include "command/list_family.h"
#include "server/server.h"
#include "protocol/resp_parser.h"
#include <cstdlib>
#include <sstream>
#include <vector>
#include <algorithm>

static Database* g_db = nullptr;

namespace {
	Database* GetDatabase() {
		return g_db;
	}
} // namespace

void ListFamily::SetDatabase(Database* db) {
	g_db = db;
}

void ListFamily::Register(CommandRegistry* registry) {
	registry->register_command("LPUSH", LPush);
	registry->register_command("RPUSH", RPush);
	registry->register_command("LPOP", LPop);
	registry->register_command("RPOP", RPop);
	registry->register_command("LLEN", LLen);
	registry->register_command("LINDEX", LIndex);
	registry->register_command("LSET", LSet);
	registry->register_command("LRANGE", LRange);
	registry->register_command("LTRIM", LTrim);
	registry->register_command("LREM", LRem);
	registry->register_command("LINSERT", LInsert);
}

std::string ListFamily::LPush(const std::vector<CompactObj>& args) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for LPUSH");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		if (list_obj != nullptr && !list_obj->isList()) {
			db->Del(key);
		}
		auto list = new std::deque<CompactObj>();
		CompactObj new_list = CompactObj::fromList();
		new_list.setObj(list);
		db->Set(key, std::move(new_list));
		list_obj = db->Find(key);
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	for (size_t i = 1; i < args.size(); i++) {
		list->push_front(args[i]);
	}

	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

std::string ListFamily::RPush(const std::vector<CompactObj>& args) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for RPUSH");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		if (list_obj != nullptr && !list_obj->isList()) {
			db->Del(key);
		}
		auto list = new std::deque<CompactObj>();
		CompactObj new_list = CompactObj::fromList();
		new_list.setObj(list);
		db->Set(key, std::move(new_list));
		list_obj = db->Find(key);
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	for (size_t i = 1; i < args.size(); i++) {
		list->push_back(args[i]);
	}

	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

std::string ListFamily::LPop(const std::vector<CompactObj>& args) {
	if (args.size() < 1 || args.size() > 2) {
		return RESPParser::make_error("wrong number of arguments for LPOP");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_null_bulk_string();
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	if (list->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t count = 1;
	if (args.size() == 2) {
		if (!ParseLongLong(args[1].toString(), &count) || count < 0) {
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

std::string ListFamily::RPop(const std::vector<CompactObj>& args) {
	if (args.size() < 1 || args.size() > 2) {
		return RESPParser::make_error("wrong number of arguments for RPOP");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_null_bulk_string();
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	if (list->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t count = 1;
	if (args.size() == 2) {
		if (!ParseLongLong(args[1].toString(), &count) || count < 0) {
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

std::string ListFamily::LLen(const std::vector<CompactObj>& args) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for LLEN");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_integer(0);
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();
	return RESPParser::make_integer(static_cast<int64_t>(list->size()));
}

std::string ListFamily::LIndex(const std::vector<CompactObj>& args) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for LINDEX");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t index;
	if (!ParseLongLong(args[1].toString(), &index)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	if (index < 0) {
		index += list->size();
	}

	if (index < 0 || index >= static_cast<int64_t>(list->size())) {
		return RESPParser::make_null_bulk_string();
	}

	return RESPParser::make_bulk_string(list->at(index).toString());
}

std::string ListFamily::LSet(const std::vector<CompactObj>& args) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for LSET");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_error("no such key");
	}

	int64_t index;
	if (!ParseLongLong(args[1].toString(), &index)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	if (index < 0) {
		index += list->size();
	}

	if (index < 0 || index >= static_cast<int64_t>(list->size())) {
		return RESPParser::make_error("index out of range");
	}

	list->at(index) = args[2];
	return RESPParser::make_simple_string("OK");
}

std::string ListFamily::LRange(const std::vector<CompactObj>& args) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for LRANGE");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_array(0);
	}

	int64_t start, stop;
	if (!ParseLongLong(args[1].toString(), &start) || !ParseLongLong(args[2].toString(), &stop)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();
	int64_t len = list->size();

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

std::string ListFamily::LTrim(const std::vector<CompactObj>& args) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for LTRIM");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_simple_string("OK");
	}

	int64_t start, stop;
	if (!ParseLongLong(args[1].toString(), &start) || !ParseLongLong(args[2].toString(), &stop)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();
	int64_t len = list->size();

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
		return RESPParser::make_simple_string("OK");
	}

	if (stop >= len) {
		stop = len - 1;
	}

	std::deque<CompactObj> trimmed;
	for (int64_t i = start; i <= stop; i++) {
		trimmed.push_back(list->at(i));
	}
	*list = std::move(trimmed);

	return RESPParser::make_simple_string("OK");
}

std::string ListFamily::LRem(const std::vector<CompactObj>& args) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for LREM");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_integer(0);
	}

	int64_t count;
	if (!ParseLongLong(args[1].toString(), &count)) {
		return RESPParser::make_error("value is not an integer or out of range");
	}

	auto list = list_obj->getObj<std::deque<CompactObj>>();
	std::string value = args[2].toString();

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

std::string ListFamily::LInsert(const std::vector<CompactObj>& args) {
	if (args.size() != 4) {
		return RESPParser::make_error("wrong number of arguments for LINSERT");
	}

	auto* db = GetDatabase();
	const CompactObj& key = args[0];
	auto* list_obj = db->Find(key);

	if (list_obj == nullptr || !list_obj->isList()) {
		return RESPParser::make_integer(0);
	}

	std::string where = args[1].toString();
	if (where != "BEFORE" && where != "AFTER") {
		return RESPParser::make_error("syntax error");
	}

	std::string pivot = args[2].toString();
	std::string value = args[3].toString();

	auto list = list_obj->getObj<std::deque<CompactObj>>();

	auto it = std::find_if(list->begin(), list->end(), [&pivot](const CompactObj& obj) {
		return obj.toString() == pivot;
	});

	if (it == list->end()) {
		return RESPParser::make_integer(-1);
	}

	if (where == "BEFORE") {
		list->insert(it, CompactObj::fromKey(value));
	} else {
		list->insert(it + 1, CompactObj::fromKey(value));
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
