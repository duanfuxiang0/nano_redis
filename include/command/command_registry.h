#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <absl/container/flat_hash_map.h>
#include <absl/strings/string_view.h>
#include "core/nano_obj.h"

struct CommandContext;

class CommandRegistry {
public:
	enum CommandFlag : uint32_t {
		kCmdFlagNone = 0,
		kCmdFlagReadOnly = 1u << 0,
		kCmdFlagWrite = 1u << 1,
		kCmdFlagAdmin = 1u << 2,
		kCmdFlagMultiKey = 1u << 3,
		kCmdFlagNoKey = 1u << 4,
	};

	struct CommandMeta {
		// Redis arity semantics:
		// arity > 0  => exact argument count
		// arity < 0  => minimum argument count is -arity
		int32_t arity = 0;
		int32_t first_key = 0;
		int32_t last_key = 0;
		int32_t key_step = 0;
		uint32_t flags = kCmdFlagNone;
	};

	using CommandHandler = std::function<std::string(const std::vector<NanoObj>&)>;
	using CommandHandlerWithContext = std::function<std::string(const std::vector<NanoObj>&, CommandContext*)>;

	static CommandRegistry& Instance();

	void RegisterCommand(const std::string& name, CommandHandler handler);
	void RegisterCommand(const std::string& name, CommandHandler handler, const CommandMeta& meta);
	void RegisterCommandWithContext(const std::string& name, CommandHandlerWithContext handler);
	void RegisterCommandWithContext(const std::string& name, CommandHandlerWithContext handler,
	                                const CommandMeta& meta);
	std::string Execute(const std::vector<NanoObj>& args, CommandContext* ctx = nullptr);
	const CommandMeta* FindMeta(absl::string_view name) const;
	std::string BuildCommandInfoResponse() const;

private:
	CommandRegistry() = default;

	struct CaseInsensitiveHash {
		using is_transparent = void;

		size_t operator()(absl::string_view s) const noexcept {
			// 64-bit FNV-1a over ASCII uppercased bytes (no allocations).
			uint64_t h = 14695981039346656037ULL;
			for (unsigned char c : s) {
				unsigned char up = (c >= 'a' && c <= 'z') ? static_cast<unsigned char>(c - 'a' + 'A') : c;
				h ^= up;
				h *= 1099511628211ULL;
			}
			return static_cast<size_t>(h);
		}

		size_t operator()(const std::string& s) const noexcept {
			return (*this)(absl::string_view {s});
		}
	};

	struct CaseInsensitiveEq {
		using is_transparent = void;

		bool operator()(const std::string& a, const std::string& b) const noexcept {
			return (*this)(absl::string_view {a}, absl::string_view {b});
		}

		bool operator()(absl::string_view a, absl::string_view b) const noexcept {
			if (a.size() != b.size()) {
				return false;
			}
			for (size_t i = 0; i < a.size(); ++i) {
				unsigned char ca = static_cast<unsigned char>(a[i]);
				unsigned char cb = static_cast<unsigned char>(b[i]);
				unsigned char ua = (ca >= 'a' && ca <= 'z') ? static_cast<unsigned char>(ca - 'a' + 'A') : ca;
				unsigned char ub = (cb >= 'a' && cb <= 'z') ? static_cast<unsigned char>(cb - 'a' + 'A') : cb;
				if (ua != ub) {
					return false;
				}
			}
			return true;
		}

		bool operator()(const std::string& a, absl::string_view b) const noexcept {
			return (*this)(absl::string_view {a}, b);
		}

		bool operator()(absl::string_view a, const std::string& b) const noexcept {
			return (*this)(a, absl::string_view {b});
		}
	};

	absl::flat_hash_map<std::string, CommandHandler, CaseInsensitiveHash, CaseInsensitiveEq> handlers;
	absl::flat_hash_map<std::string, CommandHandlerWithContext, CaseInsensitiveHash, CaseInsensitiveEq>
	    handlers_with_context;
	absl::flat_hash_map<std::string, CommandMeta, CaseInsensitiveHash, CaseInsensitiveEq> command_meta;
};
