#pragma once

#include <string>
#include <vector>
#include <deque>
#include "command/command_registry.h"
#include "core/compact_obj.h"
#include "core/database.h"

class Database;

class ListFamily {
public:
	static void Register(CommandRegistry* registry);
	static void SetDatabase(Database* db);

	static std::string LPush(const std::vector<CompactObj>& args);
	static std::string RPush(const std::vector<CompactObj>& args);
	static std::string LPop(const std::vector<CompactObj>& args);
	static std::string RPop(const std::vector<CompactObj>& args);
	static std::string LLen(const std::vector<CompactObj>& args);
	static std::string LIndex(const std::vector<CompactObj>& args);
	static std::string LSet(const std::vector<CompactObj>& args);
	static std::string LRange(const std::vector<CompactObj>& args);
	static std::string LTrim(const std::vector<CompactObj>& args);
	static std::string LRem(const std::vector<CompactObj>& args);
	static std::string LInsert(const std::vector<CompactObj>& args);

private:
	static bool ParseLongLong(const std::string& s, int64_t* out);
};
