#pragma once

#include <string>
#include <vector>
#include "command/command_registry.h"
#include "core/compact_obj.h"

struct CommandContext;

class StringFamily {
public:
	static void Register(CommandRegistry* registry);

	static std::string Set(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Get(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Del(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Exists(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string MSet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string MGet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Incr(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Decr(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string IncrBy(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string DecrBy(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Append(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string StrLen(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string GetRange(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string SetRange(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Select(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string Keys(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string FlushDB(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string DBSize(const std::vector<CompactObj>& args, CommandContext* ctx);
	static void ClearDatabase(CommandContext* ctx);
	static std::string Hello(const std::vector<CompactObj>& args);

private:
	static int64_t ParseInt(const std::string& s);
	static int AdjustIndex(int index, int length);
};
