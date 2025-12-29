#pragma once

#include <string>
#include <vector>
#include <functional>
#include <absl/container/flat_hash_map.h>

class CommandRegistry {
public:
	using CommandHandler = std::function<std::string(const std::vector<std::string>&)>;

	static CommandRegistry& instance();

	void register_command(const std::string& name, CommandHandler handler);
	std::string execute(const std::vector<std::string>& args);

private:
	CommandRegistry() = default;
	absl::flat_hash_map<std::string, CommandHandler> handlers_;
};
