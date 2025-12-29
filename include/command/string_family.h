#pragma once

#include <string>
#include <vector>
#include "command/command_registry.h"

class Database;

class StringFamily {
public:
	static void Register(CommandRegistry* registry);
	static void SetDatabase(Database* db);

	static std::string Set(const std::vector<std::string>& args);
	static std::string Get(const std::vector<std::string>& args);
	static std::string Del(const std::vector<std::string>& args);
	static std::string Exists(const std::vector<std::string>& args);
	static std::string MSet(const std::vector<std::string>& args);
	static std::string MGet(const std::vector<std::string>& args);
	static std::string Incr(const std::vector<std::string>& args);
	static std::string Decr(const std::vector<std::string>& args);
	static std::string IncrBy(const std::vector<std::string>& args);
	static std::string DecrBy(const std::vector<std::string>& args);
	static std::string Append(const std::vector<std::string>& args);
	static std::string StrLen(const std::vector<std::string>& args);
	static std::string GetRange(const std::vector<std::string>& args);
	static std::string SetRange(const std::vector<std::string>& args);
	static std::string Select(const std::vector<std::string>& args);
	static std::string Keys(const std::vector<std::string>& args);
	static void ClearDatabase();

private:
	static int64_t ParseInt(const std::string& s);
	static std::string IntToString(int64_t value);
	static int AdjustIndex(int index, int length);
};
