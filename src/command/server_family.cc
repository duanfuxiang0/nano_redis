#include "command/server_family.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gflags/gflags.h>
#include <unistd.h>

#include "core/command_context.h"
#include "core/database.h"
#include "core/rdb_serializer.h"
#include "core/util.h"
#include "protocol/resp_parser.h"
#include "server/connection.h"
#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "server/proactor_pool.h"
#include "server/slice_snapshot.h"

#include <photon/thread/thread.h>
#include <photon/thread/thread11.h>
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>

DECLARE_int32(port);
DECLARE_int32(num_shards);
DECLARE_bool(tcp_nodelay);
DECLARE_bool(use_iouring_tcp_server);
DECLARE_uint64(photon_handler_stack_kb);

namespace {

using CommandMeta = CommandRegistry::CommandMeta;
constexpr uint32_t kReadOnly = CommandRegistry::kCmdFlagReadOnly;
constexpr uint32_t kWrite = CommandRegistry::kCmdFlagWrite;
constexpr uint32_t kAdmin = CommandRegistry::kCmdFlagAdmin;
constexpr uint32_t kNoKey = CommandRegistry::kCmdFlagNoKey;

const std::chrono::steady_clock::time_point kServerStartTime = std::chrono::steady_clock::now();

class FileSink : public io::Sink {
public:
	explicit FileSink(std::string file_path) : path_(std::move(file_path)), file_(path_, std::ios::binary | std::ios::trunc) {
	}

	std::error_code Append(const uint8_t* data, size_t len) override {
		if (!file_.good()) {
			return std::make_error_code(std::errc::io_error);
		}
		file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
		if (!file_) {
			return std::make_error_code(std::errc::io_error);
		}
		return {};
	}

	std::error_code FlushAndClose() {
		if (!file_.is_open()) {
			return {};
		}
		file_.flush();
		if (!file_) {
			return std::make_error_code(std::errc::io_error);
		}
		file_.close();
		return {};
	}

	bool IsOpen() const {
		return file_.is_open();
	}

	const std::string& Path() const {
		return path_;
	}

private:
	std::string path_;
	std::ofstream file_;
};

std::error_code SaveToFile(const std::string& file_path, CommandContext* ctx) {
	const std::string tmp_path = file_path + ".tmp";
	FileSink sink(tmp_path);
	if (!sink.IsOpen()) {
		return std::make_error_code(std::errc::io_error);
	}

	RdbSerializer serializer(&sink, 0, static_cast<uint32_t>(ctx->GetShardCount()));
	auto ec = serializer.SaveHeader();
	if (ec) {
		return ec;
	}

	for (size_t db_index = 0; db_index < Database::kNumDBs; ++db_index) {
		for (size_t shard_id = 0; shard_id < ctx->GetShardCount(); ++shard_id) {
			std::error_code shard_error;
			if (ctx->IsSingleShard()) {
				auto* db = ctx->GetDB();
				if (db == nullptr) {
					shard_error = std::make_error_code(std::errc::invalid_argument);
				} else {
					db->ForEachInDB(db_index, [&shard_error, db_index, &serializer](const NanoObj& key, const NanoObj& value, int64_t expire_ms) {
						if (shard_error) {
							return;
						}
						shard_error = serializer.SaveEntry(key, value, expire_ms, static_cast<uint32_t>(db_index));
					});
				}
			} else {
				shard_error = ctx->shard_set->Await(shard_id, [db_index, &serializer]() -> std::error_code {
					auto* shard = EngineShard::Tlocal();
					if (shard == nullptr) {
						return std::make_error_code(std::errc::invalid_argument);
					}
					Database* db = &shard->GetDB();
					std::error_code shard_db_error;
					db->ForEachInDB(db_index, [&shard_db_error, db_index, &serializer](const NanoObj& key, const NanoObj& value, int64_t expire_ms) {
						if (shard_db_error) {
							return;
						}
						shard_db_error = serializer.SaveEntry(key, value, expire_ms, static_cast<uint32_t>(db_index));
					});
					return shard_db_error;
				});
			}
			if (shard_error) {
				return shard_error;
			}
		}
	}

	ec = serializer.SaveFooter();
	if (ec) {
		return ec;
	}
	ec = sink.FlushAndClose();
	if (ec) {
		return ec;
	}
	if (std::rename(sink.Path().c_str(), file_path.c_str()) != 0) {
		return std::make_error_code(std::errc::io_error);
	}
	return {};
}

char ToUpperAscii(char c) {
	return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

bool GlobMatchCaseInsensitive(std::string_view pattern, std::string_view text) {
	size_t pattern_index = 0;
	size_t text_index = 0;
	size_t star_pattern = std::string_view::npos;
	size_t star_text = std::string_view::npos;

	while (text_index < text.size()) {
		if (pattern_index < pattern.size() &&
		    (pattern[pattern_index] == '?' || ToUpperAscii(pattern[pattern_index]) == ToUpperAscii(text[text_index]))) {
			++pattern_index;
			++text_index;
			continue;
		}
		if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
			star_pattern = pattern_index++;
			star_text = text_index;
			continue;
		}
		if (star_pattern != std::string_view::npos) {
			pattern_index = star_pattern + 1;
			text_index = ++star_text;
			continue;
		}
		return false;
	}

	while (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
		++pattern_index;
	}
	return pattern_index == pattern.size();
}

std::optional<bool> ParseBool(std::string_view value) {
	if (EqualsIgnoreCase(value, "1") || EqualsIgnoreCase(value, "YES") || EqualsIgnoreCase(value, "TRUE") ||
	    EqualsIgnoreCase(value, "ON")) {
		return true;
	}
	if (EqualsIgnoreCase(value, "0") || EqualsIgnoreCase(value, "NO") || EqualsIgnoreCase(value, "FALSE") ||
	    EqualsIgnoreCase(value, "OFF")) {
		return false;
	}
	return std::nullopt;
}

std::optional<uint64_t> ParseUint64(std::string_view value) {
	uint64_t parsed = 0;
	const char* start = value.data();
	const char* end = value.data() + value.size();
	auto [ptr, ec] = std::from_chars(start, end, parsed, 10);
	if (ec != std::errc() || ptr != end) {
		return std::nullopt;
	}
	return parsed;
}

std::optional<uint64_t> ParseClientId(const NanoObj& arg) {
	const std::string_view sv = arg.GetStringView();
	if (!sv.empty()) {
		return ParseUint64(sv);
	}
	return ParseUint64(arg.ToString());
}

std::optional<uint64_t> ParseUint64Arg(const NanoObj& arg) {
	const std::string_view sv = arg.GetStringView();
	if (!sv.empty()) {
		return ParseUint64(sv);
	}
	return ParseUint64(arg.ToString());
}

std::string BuildConfigArrayResponse(const std::vector<std::pair<std::string, std::string>>& items) {
	std::string response = RESPParser::MakeArray(static_cast<int64_t>(items.size() * 2U));
	for (const auto& [key, value] : items) {
		response += RESPParser::MakeBulkString(key);
		response += RESPParser::MakeBulkString(value);
	}
	return response;
}

std::string BuildInfoPayload(std::string_view section, CommandContext* ctx) {
	std::string payload;

	const bool all_sections =
	    section.empty() || EqualsIgnoreCase(section, "ALL") || EqualsIgnoreCase(section, "DEFAULT");
	const bool server_section = all_sections || EqualsIgnoreCase(section, "SERVER");
	const bool keyspace_section = all_sections || EqualsIgnoreCase(section, "KEYSPACE");

	if (server_section) {
		const auto uptime =
		    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - kServerStartTime)
		        .count();
		payload += "# Server\r\n";
		payload += "redis_version:nano_redis_1.1\r\n";
		payload += "redis_mode:standalone\r\n";
		payload += "process_id:" + std::to_string(getpid()) + "\r\n";
		payload += "tcp_port:" + std::to_string(FLAGS_port) + "\r\n";
		payload += "uptime_in_seconds:" + std::to_string(uptime) + "\r\n";
		payload += "uptime_in_days:" + std::to_string(uptime / 86400) + "\r\n";
	}

	if (keyspace_section && ctx != nullptr) {
		const size_t db_index = ctx->GetDBIndex();
		size_t key_count = 0;
		if (ctx->shard_set != nullptr && !ctx->IsSingleShard()) {
			for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
				key_count += ctx->shard_set->Await(shard_id, [db_index]() -> size_t {
					EngineShard* shard = EngineShard::Tlocal();
					if (shard == nullptr) {
						return 0;
					}
					auto& db = shard->GetDB();
					(void)db.Select(db_index);
					return db.KeyCount();
				});
			}
		} else {
			Database* db = ctx->GetDB();
			if (db != nullptr) {
				key_count = db->KeyCount();
			}
		}

		payload += "# Keyspace\r\n";
		payload += "db" + std::to_string(db_index) + ":keys=" + std::to_string(key_count) + "\r\n";
	}

	return payload;
}

std::string BuildTimeArrayResponse() {
	const auto now = std::chrono::system_clock::now();
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
	const int64_t seconds = micros / 1000000;
	const int64_t microseconds_part = micros % 1000000;

	std::string response = RESPParser::MakeArray(2);
	response += RESPParser::MakeBulkString(std::to_string(seconds));
	response += RESPParser::MakeBulkString(std::to_string(microseconds_part));
	return response;
}

size_t PickRandomIndex(size_t upper_bound_exclusive) {
	static thread_local std::mt19937_64 generator(std::random_device {}());
	std::uniform_int_distribution<size_t> distribution(0, upper_bound_exclusive - 1);
	return distribution(generator);
}

std::string BuildClientListLine(const ProactorPool::ClientSnapshot& snapshot) {
	std::string line;
	line.reserve(128 + snapshot.client_name.size() + snapshot.last_command.size());
	line += "id=" + std::to_string(snapshot.client_id);
	line += " addr=unknown";
	line += " laddr=unknown";
	line += " name=" + snapshot.client_name;
	line += " age=" + std::to_string(snapshot.age_sec);
	line += " idle=" + std::to_string(snapshot.idle_sec);
	line += " flags=" + std::string(snapshot.close_requested ? "x" : "N");
	line += " db=" + std::to_string(snapshot.db_index);
	line += " cmd=" + snapshot.last_command;
	return line;
}

ProactorPool::ClientSnapshot MakeCurrentSnapshot(const Connection& connection) {
	const int64_t now_ms =
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
	        .count();
	ProactorPool::ClientSnapshot snapshot;
	snapshot.client_id = connection.GetClientId();
	snapshot.db_index = connection.GetDBIndex();
	snapshot.client_name = connection.GetClientName();
	snapshot.last_command = connection.GetLastCommand();
	snapshot.age_sec = std::max<int64_t>(0, (now_ms - connection.GetConnectedAtMs()) / 1000);
	snapshot.idle_sec = std::max<int64_t>(0, (now_ms - connection.GetLastActiveAtMs()) / 1000);
	snapshot.close_requested = connection.IsCloseRequested();
	return snapshot;
}

std::vector<ProactorPool::ClientSnapshot> CollectClientSnapshots(CommandContext* ctx) {
	if (ctx == nullptr || ctx->shard_set == nullptr || ctx->IsSingleShard()) {
		return ProactorPool::ListLocalConnections();
	}

	std::vector<ProactorPool::ClientSnapshot> snapshots;
	for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
		auto shard_snapshots = ctx->shard_set->Await(shard_id, []() -> std::vector<ProactorPool::ClientSnapshot> {
			return ProactorPool::ListLocalConnections();
		});
		snapshots.insert(snapshots.end(), shard_snapshots.begin(), shard_snapshots.end());
	}
	return snapshots;
}

bool KillClientById(uint64_t client_id, CommandContext* ctx) {
	if (ctx == nullptr || ctx->shard_set == nullptr || ctx->IsSingleShard()) {
		return ProactorPool::KillLocalConnectionById(client_id);
	}

	bool killed = false;
	for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
		const bool shard_killed = ctx->shard_set->Await(
		    shard_id, [client_id]() -> bool { return ProactorPool::KillLocalConnectionById(client_id); });
		if (shard_killed) {
			killed = true;
		}
	}
	return killed;
}

std::error_code BgSaveToFile(const std::string& file_path, CommandContext* ctx) {
	const std::string tmp_path = file_path + ".tmp";
	FileSink sink(tmp_path);
	if (!sink.IsOpen()) {
		return std::make_error_code(std::errc::io_error);
	}

	const uint64_t epoch = ServerFamily::snapshot_epoch_.fetch_add(1) + 1;
	const uint32_t shard_count = static_cast<uint32_t>(ctx->GetShardCount());
	RdbSerializer serializer(&sink, 0, shard_count);
	auto ec = serializer.SaveHeader();
	if (ec) {
		return ec;
	}

	for (size_t shard_id = 0; shard_id < ctx->GetShardCount(); ++shard_id) {
		if (ctx->IsSingleShard()) {
			auto* db = ctx->GetDB();
			if (db == nullptr) {
				return std::make_error_code(std::errc::invalid_argument);
			}
			SliceSnapshot snapshot(db, &serializer, epoch);
			ec = snapshot.SerializeAllDBs();
			if (ec) {
				return ec;
			}
		} else {
			ec = ctx->shard_set->Await(shard_id,
			    [&serializer, epoch]() -> std::error_code {
				    auto* shard = EngineShard::Tlocal();
				    if (shard == nullptr) {
					    return std::make_error_code(std::errc::invalid_argument);
				    }
				    SliceSnapshot snapshot(&shard->GetDB(), &serializer, epoch);
				    return snapshot.SerializeAllDBs();
			    });
			if (ec) {
				return ec;
			}
		}
	}

	ec = serializer.SaveFooter();
	if (ec) {
		return ec;
	}
	ec = sink.FlushAndClose();
	if (ec) {
		return ec;
	}
	if (std::rename(sink.Path().c_str(), file_path.c_str()) != 0) {
		return std::make_error_code(std::errc::io_error);
	}
	return {};
}

} // namespace

std::atomic<bool> ServerFamily::bg_save_in_progress_ {false};
std::atomic<uint64_t> ServerFamily::snapshot_epoch_ {0};

bool ServerFamily::IsBgSaveInProgress() {
	return bg_save_in_progress_.load(std::memory_order_relaxed);
}

void ServerFamily::Register(CommandRegistry* registry) {
	registry->RegisterCommandWithContext(
	    "INFO", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Info(args, ctx); },
	    CommandMeta {-1, 0, 0, 0, kReadOnly | kAdmin | kNoKey});
	registry->RegisterCommandWithContext(
	    "CONFIG", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Config(args, ctx); },
	    CommandMeta {-2, 0, 0, 0, kAdmin | kNoKey | kWrite});
	registry->RegisterCommandWithContext(
	    "CLIENT", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Client(args, ctx); },
	    CommandMeta {-2, 0, 0, 0, kAdmin | kNoKey | kWrite});
	registry->RegisterCommandWithContext(
	    "TIME", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Time(args, ctx); },
	    CommandMeta {1, 0, 0, 0, kReadOnly | kNoKey});
	registry->RegisterCommandWithContext(
	    "RANDOMKEY", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return RandomKey(args, ctx); },
	    CommandMeta {1, 0, 0, 0, kReadOnly | kNoKey});
	registry->RegisterCommandWithContext(
	    "SAVE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return Save(args, ctx); },
	    CommandMeta {-2, 0, 0, 0, kAdmin | kNoKey | kWrite});
	registry->RegisterCommandWithContext(
	    "BGSAVE", [](const std::vector<NanoObj>& args, CommandContext* ctx) { return BgSave(args, ctx); },
	    CommandMeta {-2, 0, 0, 0, kAdmin | kNoKey | kWrite});
}

std::string ServerFamily::Info(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() > 2) {
		return RESPParser::MakeError("wrong number of arguments for 'INFO'");
	}

	const std::string_view section = (args.size() == 2) ? args[1].GetStringView() : std::string_view {};
	std::string payload = BuildInfoPayload(section, ctx);
	return RESPParser::MakeBulkString(payload);
}

std::string ServerFamily::Config(const std::vector<NanoObj>& args, CommandContext* ctx) {
	(void)ctx;
	if (args.size() < 2) {
		return RESPParser::MakeError("wrong number of arguments for 'CONFIG'");
	}

	const std::string_view subcmd = args[1].GetStringView();
	if (EqualsIgnoreCase(subcmd, "GET")) {
		if (args.size() != 3) {
			return RESPParser::MakeError("wrong number of arguments for 'CONFIG GET'");
		}
		const std::string pattern = args[2].ToString();
		const std::array<std::pair<std::string, std::string>, 5> options = {
		    std::make_pair("port", std::to_string(FLAGS_port)),
		    std::make_pair("num_shards", std::to_string(FLAGS_num_shards)),
		    std::make_pair("tcp_nodelay", FLAGS_tcp_nodelay ? "yes" : "no"),
		    std::make_pair("use_iouring_tcp_server", FLAGS_use_iouring_tcp_server ? "yes" : "no"),
		    std::make_pair("photon_handler_stack_kb", std::to_string(FLAGS_photon_handler_stack_kb)),
		};

		std::vector<std::pair<std::string, std::string>> matched;
		for (const auto& option : options) {
			if (GlobMatchCaseInsensitive(pattern, option.first)) {
				matched.push_back(option);
			}
		}
		return BuildConfigArrayResponse(matched);
	}

	if (EqualsIgnoreCase(subcmd, "SET")) {
		if (args.size() != 4) {
			return RESPParser::MakeError("wrong number of arguments for 'CONFIG SET'");
		}
		const std::string_view name = args[2].GetStringView();
		const std::string_view value = args[3].GetStringView();

		if (EqualsIgnoreCase(name, "tcp_nodelay")) {
			auto parsed = ParseBool(value);
			if (!parsed.has_value()) {
				return RESPParser::MakeError("Invalid argument for CONFIG SET 'tcp_nodelay'");
			}
			FLAGS_tcp_nodelay = *parsed;
			return RESPParser::OkResponse();
		}

		if (EqualsIgnoreCase(name, "use_iouring_tcp_server")) {
			auto parsed = ParseBool(value);
			if (!parsed.has_value()) {
				return RESPParser::MakeError("Invalid argument for CONFIG SET 'use_iouring_tcp_server'");
			}
			FLAGS_use_iouring_tcp_server = *parsed;
			return RESPParser::OkResponse();
		}

		if (EqualsIgnoreCase(name, "photon_handler_stack_kb")) {
			int64_t parsed_value = 0;
			const std::string str_value(value);
			auto [ptr, ec] = std::from_chars(str_value.data(), str_value.data() + str_value.size(), parsed_value, 10);
			if (ec != std::errc() || ptr != str_value.data() + str_value.size() || parsed_value <= 0) {
				return RESPParser::MakeError("Invalid argument for CONFIG SET 'photon_handler_stack_kb'");
			}
			FLAGS_photon_handler_stack_kb = static_cast<uint64_t>(parsed_value);
			return RESPParser::OkResponse();
		}

		return RESPParser::MakeError("Unsupported CONFIG parameter");
	}

	if (EqualsIgnoreCase(subcmd, "RESETSTAT")) {
		if (args.size() != 2) {
			return RESPParser::MakeError("wrong number of arguments for 'CONFIG RESETSTAT'");
		}
		return RESPParser::OkResponse();
	}

	return RESPParser::MakeError("Unknown CONFIG subcommand");
}

std::string ServerFamily::Client(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::MakeError("wrong number of arguments for 'CLIENT'");
	}

	const std::string_view subcmd = args[1].GetStringView();
	if (EqualsIgnoreCase(subcmd, "GETNAME")) {
		if (args.size() != 2) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT GETNAME'");
		}
		if (ctx == nullptr || ctx->connection == nullptr) {
			return RESPParser::MakeError("CLIENT command not available in this context");
		}
		const std::string& client_name = ctx->connection->GetClientName();
		if (client_name.empty()) {
			return RESPParser::NullBulkResponse();
		}
		return RESPParser::MakeBulkString(client_name);
	}

	if (EqualsIgnoreCase(subcmd, "SETNAME")) {
		if (args.size() != 3) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT SETNAME'");
		}
		if (ctx == nullptr || ctx->connection == nullptr) {
			return RESPParser::MakeError("CLIENT command not available in this context");
		}
		ctx->connection->SetClientName(args[2].ToString());
		return RESPParser::OkResponse();
	}

	if (EqualsIgnoreCase(subcmd, "ID")) {
		if (args.size() != 2) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT ID'");
		}
		if (ctx == nullptr || ctx->connection == nullptr) {
			return RESPParser::MakeError("CLIENT command not available in this context");
		}
		return RESPParser::MakeInteger(static_cast<int64_t>(ctx->connection->GetClientId()));
	}

	if (EqualsIgnoreCase(subcmd, "INFO")) {
		if (args.size() != 2) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT INFO'");
		}
		if (ctx == nullptr || ctx->connection == nullptr) {
			return RESPParser::MakeError("CLIENT command not available in this context");
		}
		const ProactorPool::ClientSnapshot snapshot = MakeCurrentSnapshot(*ctx->connection);
		return RESPParser::MakeBulkString(BuildClientListLine(snapshot));
	}

	if (EqualsIgnoreCase(subcmd, "LIST")) {
		if (args.size() != 2) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT LIST'");
		}

		std::vector<ProactorPool::ClientSnapshot> snapshots = CollectClientSnapshots(ctx);
		if (snapshots.empty() && ctx != nullptr && ctx->connection != nullptr) {
			snapshots.push_back(MakeCurrentSnapshot(*ctx->connection));
		}
		std::sort(snapshots.begin(), snapshots.end(),
		          [](const ProactorPool::ClientSnapshot& lhs, const ProactorPool::ClientSnapshot& rhs) {
			          return lhs.client_id < rhs.client_id;
		          });

		std::string payload;
		for (size_t i = 0; i < snapshots.size(); ++i) {
			payload += BuildClientListLine(snapshots[i]);
			if (i + 1 != snapshots.size()) {
				payload.push_back('\n');
			}
		}
		return RESPParser::MakeBulkString(payload);
	}

	if (EqualsIgnoreCase(subcmd, "KILL")) {
		if (args.size() != 3 && args.size() != 4) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT KILL'");
		}

		std::optional<uint64_t> client_id;
		if (args.size() == 3) {
			client_id = ParseClientId(args[2]);
		} else {
			if (!EqualsIgnoreCase(args[2].GetStringView(), "ID")) {
				return RESPParser::MakeError("syntax error");
			}
			client_id = ParseClientId(args[3]);
		}
		if (!client_id.has_value()) {
			return RESPParser::MakeError("invalid client id");
		}

		const bool killed = KillClientById(*client_id, ctx);
		return RESPParser::MakeInteger(killed ? 1 : 0);
	}

	if (EqualsIgnoreCase(subcmd, "PAUSE")) {
		if (args.size() != 3 && args.size() != 4) {
			return RESPParser::MakeError("wrong number of arguments for 'CLIENT PAUSE'");
		}

		auto timeout_ms = ParseUint64Arg(args[2]);
		if (!timeout_ms.has_value()) {
			return RESPParser::MakeError("timeout is not an integer or out of range");
		}
		if (args.size() == 4) {
			const std::string_view mode = args[3].GetStringView();
			if (!EqualsIgnoreCase(mode, "ALL") && !EqualsIgnoreCase(mode, "WRITE")) {
				return RESPParser::MakeError("unsupported CLIENT PAUSE mode");
			}
		}

		ProactorPool::PauseClients(*timeout_ms);
		return RESPParser::OkResponse();
	}

	return RESPParser::MakeError("Unknown CLIENT subcommand");
}

std::string ServerFamily::Time(const std::vector<NanoObj>& args, CommandContext* ctx) {
	(void)ctx;
	if (args.size() != 1) {
		return RESPParser::MakeError("wrong number of arguments for 'TIME'");
	}
	return BuildTimeArrayResponse();
}

std::string ServerFamily::RandomKey(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() != 1) {
		return RESPParser::MakeError("wrong number of arguments for 'RANDOMKEY'");
	}
	if (ctx == nullptr) {
		return RESPParser::MakeError("ERR internal context");
	}

	if (ctx->shard_set == nullptr || ctx->IsSingleShard()) {
		Database* db = ctx->GetDB();
		if (db == nullptr) {
			return RESPParser::MakeError("ERR internal database");
		}
		std::vector<std::string> keys = db->Keys();
		if (keys.empty()) {
			return RESPParser::NullBulkResponse();
		}
		const size_t random_index = PickRandomIndex(keys.size());
		return RESPParser::MakeBulkString(keys[random_index]);
	}

	std::vector<std::string> all_keys;
	for (size_t shard_id = 0; shard_id < ctx->shard_set->Size(); ++shard_id) {
		auto shard_keys = ctx->shard_set->Await(shard_id, [db_index = ctx->GetDBIndex()]() -> std::vector<std::string> {
			EngineShard* shard = EngineShard::Tlocal();
			if (shard == nullptr) {
				return {};
			}
			auto& db = shard->GetDB();
			(void)db.Select(db_index);
			return db.Keys();
		});
		all_keys.insert(all_keys.end(), shard_keys.begin(), shard_keys.end());
	}

	if (all_keys.empty()) {
		return RESPParser::NullBulkResponse();
	}
	const size_t random_index = PickRandomIndex(all_keys.size());
	return RESPParser::MakeBulkString(all_keys[random_index]);
}

std::string ServerFamily::Save(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() > 2) {
		return RESPParser::MakeError("wrong number of arguments for 'SAVE'");
	}
	if (ctx == nullptr) {
		return RESPParser::MakeError("ERR internal context");
	}
	const std::string file_path = (args.size() == 2) ? args[1].ToString() : "dump.nrdb";
	auto ec = SaveToFile(file_path, ctx);
	if (ec) {
		return RESPParser::MakeError(std::string("SAVE failed: ") + ec.message());
	}
	return RESPParser::OkResponse();
}

std::string ServerFamily::BgSave(const std::vector<NanoObj>& args, CommandContext* ctx) {
	if (args.size() > 2) {
		return RESPParser::MakeError("wrong number of arguments for 'BGSAVE'");
	}
	if (ctx == nullptr) {
		return RESPParser::MakeError("ERR internal context");
	}
	if (bg_save_in_progress_.load(std::memory_order_relaxed)) {
		return RESPParser::MakeError("Background save already in progress");
	}

	const std::string file_path = (args.size() == 2) ? args[1].ToString() : "dump.nrdb";

	if (ctx->IsSingleShard() || ctx->shard_set == nullptr) {
		bg_save_in_progress_.store(true, std::memory_order_relaxed);
		auto ec = BgSaveToFile(file_path, ctx);
		bg_save_in_progress_.store(false, std::memory_order_relaxed);
		if (ec) {
			return RESPParser::MakeError(std::string("BGSAVE failed: ") + ec.message());
		}
		return RESPParser::MakeSimpleString("Background saving started");
	}

	bg_save_in_progress_.store(true, std::memory_order_relaxed);
	EngineShardSet* shard_set = ctx->shard_set;
	size_t shard_count = ctx->GetShardCount();

	(void)photon::thread_create11([file_path, shard_set, shard_count]() {
		const std::string tmp_path = file_path + ".tmp";
		FileSink sink(tmp_path);
		if (!sink.IsOpen()) {
			LOG_WARN("BGSAVE: failed to open tmp file '`'", tmp_path);
			bg_save_in_progress_.store(false, std::memory_order_relaxed);
			return;
		}

		const uint64_t epoch = snapshot_epoch_.fetch_add(1) + 1;
		RdbSerializer serializer(&sink, 0, static_cast<uint32_t>(shard_count));
		auto ec = serializer.SaveHeader();
		if (ec) {
			LOG_WARN("BGSAVE: failed to write header (`)", ec.message());
			bg_save_in_progress_.store(false, std::memory_order_relaxed);
			return;
		}

		for (size_t shard_id = 0; shard_id < shard_count; ++shard_id) {
			ec = shard_set->Await(shard_id,
			    [&serializer, epoch]() -> std::error_code {
				    auto* shard = EngineShard::Tlocal();
				    if (shard == nullptr) {
					    return std::make_error_code(std::errc::invalid_argument);
				    }
				    SliceSnapshot snapshot(&shard->GetDB(), &serializer, epoch);
				    return snapshot.SerializeAllDBs();
			    });
			if (ec) {
				LOG_WARN("BGSAVE: shard ` failed (`)", shard_id, ec.message());
				bg_save_in_progress_.store(false, std::memory_order_relaxed);
				return;
			}
		}

		ec = serializer.SaveFooter();
		if (!ec) {
			ec = sink.FlushAndClose();
		}
		if (!ec) {
			if (std::rename(sink.Path().c_str(), file_path.c_str()) != 0) {
				ec = std::make_error_code(std::errc::io_error);
			}
		}
		if (ec) {
			LOG_WARN("BGSAVE: finalize failed (`)", ec.message());
		} else {
			LOG_INFO("BGSAVE: completed successfully to '`'", file_path);
		}
		bg_save_in_progress_.store(false, std::memory_order_relaxed);
	});

	return RESPParser::MakeSimpleString("Background saving started");
}
