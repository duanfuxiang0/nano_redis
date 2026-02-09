#pragma once

#include <string>
#include <vector>

#include "command/command_registry.h"
#include "core/nano_obj.h"

struct CommandContext;

class ServerFamily {
public:
	static void Register(CommandRegistry* registry);

	static std::string Info(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Config(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Client(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string Time(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string RandomKey(const std::vector<NanoObj>& args, CommandContext* ctx);
};
