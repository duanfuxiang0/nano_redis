#pragma once

#include <atomic>
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
	static std::string Save(const std::vector<NanoObj>& args, CommandContext* ctx);
	static std::string BgSave(const std::vector<NanoObj>& args, CommandContext* ctx);
	static bool IsBgSaveInProgress();

	static std::atomic<bool> bg_save_in_progress_;
	static std::atomic<uint64_t> snapshot_epoch_;
};
