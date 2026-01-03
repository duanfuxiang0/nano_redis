#include "command/command_registry.h"
#include "core/command_context.h"
#include "core/compact_obj.h"
#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

CommandRegistry& CommandRegistry::instance() {
	static CommandRegistry registry;
	return registry;
}

void CommandRegistry::register_command(const std::string& name, CommandHandler handler) {
	handlers_[name] = handler;
}

void CommandRegistry::register_command_with_context(const std::string& name, CommandHandlerWithContext handler) {
	handlers_with_context_[name] = handler;
}

std::string CommandRegistry::execute(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.empty()) {
		return "-ERR Empty command\r\n";
	}

	const CompactObj& cmd_obj = args[0];
	std::string cmd = cmd_obj.toString();

	// Convert command to uppercase for case-insensitive matching
	std::transform(cmd.begin(), cmd.end(), cmd.begin(),
	               [](unsigned char c) { return std::toupper(c); });

	auto it_with_ctx = handlers_with_context_.find(cmd);
	if (it_with_ctx != handlers_with_context_.end()) {
		return it_with_ctx->second(args, ctx);
	}

	auto it = handlers_.find(cmd);
	if (it != handlers_.end()) {
		return it->second(args);
	} else {
		return "-ERR Unknown command '" + cmd + "'\r\n";
	}
}
