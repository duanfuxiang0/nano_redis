#pragma once

#include <string>
#include <vector>
#include <functional>
#include <absl/container/flat_hash_map.h>
#include "core/compact_obj.h"

struct CommandContext;

class CommandRegistry {
public:
	using CommandHandler = std::function<std::string(const std::vector<CompactObj>&)>;
	using CommandHandlerWithContext = std::function<std::string(const std::vector<CompactObj>&, CommandContext*)>;

	static CommandRegistry& instance();

	void register_command(const std::string& name, CommandHandler handler);
	void register_command_with_context(const std::string& name, CommandHandlerWithContext handler);
	std::string execute(const std::vector<CompactObj>& args, CommandContext* ctx = nullptr);

private:
	CommandRegistry() = default;
	absl::flat_hash_map<std::string, CommandHandler> handlers_;
	absl::flat_hash_map<std::string, CommandHandlerWithContext> handlers_with_context_;
};
