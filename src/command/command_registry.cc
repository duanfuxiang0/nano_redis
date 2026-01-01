#include "command/command_registry.h"
#include "core/compact_obj.h"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

CommandRegistry& CommandRegistry::instance() {
	static CommandRegistry registry;
	return registry;
}

void CommandRegistry::register_command(const std::string& name, CommandHandler handler) {
	handlers_[name] = handler;
}

std::string CommandRegistry::execute(const std::vector<CompactObj>& args) {
	if (args.empty()) {
		return "-ERR Empty command\r\n";
	}

	const CompactObj& cmd_obj = args[0];
	std::string cmd = cmd_obj.toString();
	auto it = handlers_.find(cmd);
	if (it != handlers_.end()) {
		return it->second(args);
	} else {
		return "-ERR Unknown command '" + cmd + "'\r\n";
	}
}
