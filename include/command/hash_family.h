#pragma once

#include <string>
#include <vector>
#include <optional>
#include "command/command_registry.h"
#include "core/compact_obj.h"
#include "core/unordered_dense.h"
#include "core/database.h"

struct CommandContext;

class HashFamily {
public:
	static void Register(CommandRegistry* registry);

	static std::string HSet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HGet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HMSet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HMGet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HDel(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HExists(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HLen(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HKeys(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HVals(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HGetAll(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HIncrBy(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HScan(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HStrLen(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string HRandField(const std::vector<CompactObj>& args, CommandContext* ctx);

private:
	static bool ParseLongLong(const std::string& s, int64_t* out);

	using HashType = ankerl::unordered_dense::map<std::string, std::string, ankerl::unordered_dense::hash<std::string>>;
};
