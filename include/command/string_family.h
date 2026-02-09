#pragma once

#include <optional>
#include <string>
#include <vector>
#include "command/command_registry.h"
#include "core/nano_obj.h"

struct CommandContext;

class StringFamily {
public:
	static void Register(CommandRegistry* registry);

	static std::string Set(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Get(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Del(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Exists(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string MSet(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string MGet(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Incr(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Decr(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string IncrBy(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string DecrBy(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Append(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string StrLen(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Type(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string GetRange(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string SetRange(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Expire(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string TTL(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Persist(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Select(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Keys(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string FlushDB(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string DBSize(const std::vector<NanoObj>& args, CommandContext* ctx);
	static void ClearDatabase(CommandContext* ctx);
	static std::string Hello(const std::vector<NanoObj>& args);

private:
	static std::optional<int64_t> ParseInt(const std::string& s);
	static int64_t AdjustIndex(int64_t index, int64_t length);
};
