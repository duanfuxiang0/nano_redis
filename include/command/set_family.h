#pragma once

#include <string>
#include <vector>
#include "command/command_registry.h"
#include "core/compact_obj.h"
#include "core/unordered_dense.h"
#include "core/database.h"

class Database;

class SetFamily {
public:
	static void Register(CommandRegistry* registry);
	static void SetDatabase(Database* db);

	static std::string SAdd(const std::vector<CompactObj>& args);
	static std::string SRem(const std::vector<CompactObj>& args);
	static std::string SPop(const std::vector<CompactObj>& args);
	static std::string SMembers(const std::vector<CompactObj>& args);
	static std::string SCard(const std::vector<CompactObj>& args);
	static std::string SIsMember(const std::vector<CompactObj>& args);
	static std::string SMIsMember(const std::vector<CompactObj>& args);
	static std::string SInter(const std::vector<CompactObj>& args);
	static std::string SUnion(const std::vector<CompactObj>& args);
	static std::string SDiff(const std::vector<CompactObj>& args);
	static std::string SScan(const std::vector<CompactObj>& args);
	static std::string SRandMember(const std::vector<CompactObj>& args);
	static std::string SMove(const std::vector<CompactObj>& args);

private:
	static bool ParseLongLong(const std::string& s, int64_t* out);

	using SetType = ankerl::unordered_dense::set<std::string, ankerl::unordered_dense::hash<std::string>>;
};
