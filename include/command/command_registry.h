#pragma once

#include <string>
#include <vector>
#include <functional>
#include <absl/container/flat_hash_map.h>
#include "core/compact_obj.h"

class CommandRegistry {
public:
	using CommandHandler = std::function<std::string(const std::vector<CompactObj>&)>;

	static CommandRegistry& instance();

	void register_command(const std::string& name, CommandHandler handler);
	std::string execute(const std::vector<CompactObj>& args);

private:
	CommandRegistry() = default;
	absl::flat_hash_map<std::string, CommandHandler> handlers_;
};
