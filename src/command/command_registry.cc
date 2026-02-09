#include "command/command_registry.h"
#include "core/command_context.h"
#include "core/nano_obj.h"
#include "protocol/resp_parser.h"
#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

namespace {

std::vector<std::string> BuildFlagStrings(const CommandRegistry::CommandMeta& meta) {
	std::vector<std::string> flags;
	if ((meta.flags & CommandRegistry::kCmdFlagReadOnly) != 0) {
		flags.emplace_back("readonly");
	}
	if ((meta.flags & CommandRegistry::kCmdFlagWrite) != 0) {
		flags.emplace_back("write");
	}
	if ((meta.flags & CommandRegistry::kCmdFlagAdmin) != 0) {
		flags.emplace_back("admin");
	}
	if ((meta.flags & CommandRegistry::kCmdFlagMultiKey) != 0) {
		flags.emplace_back("multikey");
	}
	if ((meta.flags & CommandRegistry::kCmdFlagNoKey) != 0) {
		flags.emplace_back("nokey");
	}
	return flags;
}

} // namespace

CommandRegistry& CommandRegistry::Instance() {
	static CommandRegistry registry;
	return registry;
}

void CommandRegistry::RegisterCommand(const std::string& name, CommandHandler handler) {
	RegisterCommand(name, std::move(handler), CommandMeta {});
}

void CommandRegistry::RegisterCommand(const std::string& name, CommandHandler handler, const CommandMeta& meta) {
	handlers[name] = std::move(handler);
	command_meta[name] = meta;
}

void CommandRegistry::RegisterCommandWithContext(const std::string& name, CommandHandlerWithContext handler) {
	RegisterCommandWithContext(name, std::move(handler), CommandMeta {});
}

void CommandRegistry::RegisterCommandWithContext(const std::string& name, CommandHandlerWithContext handler,
                                                 const CommandMeta& meta) {
	handlers_with_context[name] = std::move(handler);
	command_meta[name] = meta;
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

const CommandRegistry::CommandMeta* CommandRegistry::FindMeta(absl::string_view name) const {
	auto it = command_meta.find(name);
	if (it == command_meta.end()) {
		return nullptr;
	}
	return &it->second;
}

std::string CommandRegistry::BuildCommandInfoResponse() const {
	std::vector<std::tuple<std::string, CommandMeta>> rows;
	rows.reserve(command_meta.size());

	for (const auto& [name, meta] : command_meta) {
		rows.emplace_back(name, meta);
	}

	std::sort(rows.begin(), rows.end(),
	          [](const auto& lhs, const auto& rhs) { return std::get<0>(lhs) < std::get<0>(rhs); });

	std::string response = RESPParser::MakeArray(static_cast<int64_t>(rows.size()));
	for (const auto& [name, meta] : rows) {
		response += RESPParser::MakeArray(6);
		response += RESPParser::MakeBulkString(name);
		response += RESPParser::MakeInteger(meta.arity);

		const std::vector<std::string> flags = BuildFlagStrings(meta);
		response += RESPParser::MakeArray(static_cast<int64_t>(flags.size()));
		for (const auto& flag : flags) {
			response += RESPParser::MakeBulkString(flag);
		}

		response += RESPParser::MakeInteger(meta.first_key);
		response += RESPParser::MakeInteger(meta.last_key);
		response += RESPParser::MakeInteger(meta.key_step);
	}

	return response;
}
