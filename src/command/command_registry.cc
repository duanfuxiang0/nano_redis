#include "command/command_registry.h"
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

std::string CommandRegistry::execute(const std::vector<std::string>& args) {
	if (args.empty()) {
		return "-ERR Empty command\r\n";
	}

	const std::string& cmd = args[0];
	auto it = handlers_.find(cmd);
	if (it != handlers_.end()) {
		return it->second(args);
	} else {
		return "-ERR Unknown command '" + cmd + "'\r\n";
	}
}
