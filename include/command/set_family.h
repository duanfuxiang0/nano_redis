#pragma once

#include <string>
#include <vector>
#include "command/command_registry.h"
#include "core/nano_obj.h"
#include "core/unordered_dense.h"
#include "core/database.h"

struct CommandContext;

class SetFamily {
public:
	static void Register(CommandRegistry* registry);

	static std::string SAdd(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SRem(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SPop(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SMembers(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SCard(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SIsMember(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SMIsMember(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SInter(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SUnion(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SDiff(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SScan(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SRandMember(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SMove(const std::vector<NanoObj>& args, CommandContext* ctx);

private:
	static bool ParseLongLong(const std::string& s, int64_t* out);

	using SetType = ankerl::unordered_dense::set<std::string, ankerl::unordered_dense::hash<std::string>>;
};
