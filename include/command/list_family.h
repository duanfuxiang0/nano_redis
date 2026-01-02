#pragma once

#include <string>
#include <vector>
#include <deque>
#include "command/command_registry.h"
#include "core/compact_obj.h"
#include "core/database.h"

struct CommandContext;

class ListFamily {
public:
	static void Register(CommandRegistry* registry);

	static std::string LPush(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string RPush(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LPop(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string RPop(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LLen(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LIndex(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LSet(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LRange(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LTrim(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LRem(const std::vector<CompactObj>& args, CommandContext* ctx);
	static std::string LInsert(const std::vector<CompactObj>& args, CommandContext* ctx);

private:
	static bool ParseLongLong(const std::string& s, int64_t* out);
};
