#include "command/command_registry.h"
#include "core/command_context.h"
#include "core/nano_obj.h"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

CommandRegistry& CommandRegistry::Instance() {
	static CommandRegistry registry;
	return registry;
}

void CommandRegistry::RegisterCommand(const std::string& name, CommandHandler handler) {
	handlers[name] = std::move(handler);
}

void CommandRegistry::RegisterCommandWithContext(const std::string& name, CommandHandlerWithContext handler) {
	handlers_with_context[name] = std::move(handler);
}

std::string CommandRegistry::Execute(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.empty()) {
		return "-ERR Empty command\r\n";
	}

	const NanoObj& cmd_obj = args[0];
	absl::string_view cmd_sv = cmd_obj.GetStringView();
	if (cmd_sv.empty()) {
		// Fallback for non-string commands (should be rare).
		std::string cmd = cmd_obj.ToString();
		auto it_with_ctx = handlers_with_context.find(cmd);
		if (it_with_ctx != handlers_with_context.end()) {
			return it_with_ctx->second(args, ctx);
		}

		auto it = handlers.find(cmd);
		if (it != handlers.end()) {
			return it->second(args);
		}
		return "-ERR Unknown command '" + cmd + "'\r\n";
	}

	auto it_with_ctx = handlers_with_context.find(cmd_sv);
	if (it_with_ctx != handlers_with_context.end()) {
		return it_with_ctx->second(args, ctx);
	}

	auto it = handlers.find(cmd_sv);
	if (it != handlers.end()) {
		return it->second(args);
	} else {
		return "-ERR Unknown command '" + std::string(cmd_sv) + "'\r\n";
	}
}
