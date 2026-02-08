#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>
#include "command/string_family.h"
#include "command/command_registry.h"
#include "core/nano_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "server/engine_shard_set.h"
#include "server/engine_shard.h"
#include "server/sharding.h"
#include "protocol/resp_parser.h"

class MultiShardIntegrationTest : public ::testing::Test {
protected:
	void SetUp() override {
		registry = &CommandRegistry::Instance();
		StringFamily::Register(registry);
	}

	std::string ExecuteCommand(const std::string& cmd, const std::vector<std::string>& args, CommandContext* ctx) {
		std::vector<NanoObj> full_args = {NanoObj::fromKey(cmd)};
		full_args.reserve(1 + args.size());
		for (const auto& arg : args) {
			full_args.emplace_back(NanoObj::fromKey(arg));
		}
		return registry->Execute(full_args, ctx);
	}

	// Simple RESP array parser for test assertions. Extracts bulk strings from arrays.
	// Null bulk strings ($-1) and errors (-ERR) are represented as "(nil)".
	void ParseRESPArray(const std::string& resp, std::vector<std::string>& values) {
		values.clear();
		if (resp.empty() || resp[0] != '*') {
			return;
		}

		size_t pos = 1;
		auto read_line = [&]() -> std::string {
			size_t end = resp.find("\r\n", pos);
			if (end == std::string::npos) {
				return {};
			}
			std::string line = resp.substr(pos, end - pos);
			pos = end + 2;
			return line;
		};

		int32_t count = std::stoi(read_line());
		for (int32_t element_idx = 0; element_idx < count && pos < resp.size(); ++element_idx) {
			char type = resp[pos++];
			if (type == '$') {
				int32_t len = std::stoi(read_line());
				if (len < 0) {
					values.emplace_back("(nil)");
				} else {
					values.emplace_back(resp, pos, static_cast<size_t>(len));
					pos += static_cast<size_t>(len) + 2; // skip data + CRLF
				}
			} else if (type == '-') {
				read_line(); // skip error message
				values.emplace_back("(nil)");
			} else {
				read_line(); // skip other types
			}
		}
	}

	CommandRegistry* registry;
};

TEST_F(MultiShardIntegrationTest, StringOperations) {
	Database db1, db2;
	CommandContext ctx1(&db1, 0);
	CommandContext ctx2(&db2, 0);

	EXPECT_EQ(ExecuteCommand("SET", {"key1", "value1"}, &ctx1), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("SET", {"key2", "value2"}, &ctx2), "+OK\r\n");

	EXPECT_EQ(ExecuteCommand("GET", {"key1"}, &ctx1), "$6\r\nvalue1\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"key2"}, &ctx2), "$6\r\nvalue2\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"key1"}, &ctx2), "$-1\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"key2"}, &ctx1), "$-1\r\n");
}

TEST_F(MultiShardIntegrationTest, MSetAndMGet) {
	Database db;
	CommandContext ctx(&db, 0);

	EXPECT_EQ(ExecuteCommand("MSET", {"k1", "v1", "k2", "v2", "k3", "v3"}, &ctx), "+OK\r\n");

	std::string response = ExecuteCommand("MGET", {"k1", "k2", "k3", "k4"}, &ctx);
	EXPECT_TRUE(response.find("*4") == 0);

	std::vector<std::string> values;
	ParseRESPArray(response, values);
	EXPECT_EQ(values.size(), 4);
	EXPECT_EQ(values[0], "v1");
	EXPECT_EQ(values[1], "v2");
	EXPECT_EQ(values[2], "v3");
	EXPECT_EQ(values[3], "(nil)");
}

TEST_F(MultiShardIntegrationTest, MSetWithMixedKeys) {
	Database db;
	CommandContext ctx(&db, 0);

	EXPECT_EQ(ExecuteCommand("MSET", {"a", "1", "b", "2", "a", "3"}, &ctx), "+OK\r\n");

	EXPECT_EQ(ExecuteCommand("GET", {"a"}, &ctx), "$1\r\n3\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"b"}, &ctx), "$1\r\n2\r\n");
}

TEST_F(MultiShardIntegrationTest, MultipleDatabases) {
	Database db;
	CommandContext ctx0(&db, 0);
	CommandContext ctx1(&db, 1);

	EXPECT_EQ(ExecuteCommand("SET", {"key0", "value0"}, &ctx0), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("SELECT", {"1"}, &ctx0), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("SET", {"key1", "value1"}, &ctx0), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("SELECT", {"0"}, &ctx0), "+OK\r\n");

	EXPECT_EQ(ExecuteCommand("GET", {"key0"}, &ctx0), "$6\r\nvalue0\r\n");
	EXPECT_EQ(ExecuteCommand("SELECT", {"1"}, &ctx0), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"key1"}, &ctx0), "$6\r\nvalue1\r\n");
	EXPECT_EQ(ExecuteCommand("SELECT", {"0"}, &ctx0), "+OK\r\n");

	EXPECT_EQ(ExecuteCommand("GET", {"key1"}, &ctx0), "$-1\r\n");
	EXPECT_EQ(ExecuteCommand("SELECT", {"1"}, &ctx0), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"key0"}, &ctx0), "$-1\r\n");
}

TEST_F(MultiShardIntegrationTest, FlushDBAndDBSize) {
	Database db;
	CommandContext ctx(&db, 0);

	EXPECT_EQ(ExecuteCommand("SET", {"k1", "v1"}, &ctx), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("SET", {"k2", "v2"}, &ctx), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("DBSIZE", {}, &ctx), ":2\r\n");

	EXPECT_EQ(ExecuteCommand("FLUSHDB", {}, &ctx), "+OK\r\n");
	EXPECT_EQ(ExecuteCommand("DBSIZE", {}, &ctx), ":0\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"k1"}, &ctx), "$-1\r\n");
	EXPECT_EQ(ExecuteCommand("GET", {"k2"}, &ctx), "$-1\r\n");
}

TEST_F(MultiShardIntegrationTest, ShardingDistribution) {
	const uint32_t key_count = 1000;
	std::vector<std::string> keys;
	keys.reserve(key_count);
	for (uint32_t key_idx = 0; key_idx < key_count; ++key_idx) {
		keys.emplace_back("key_" + std::to_string(key_idx));
	}

	std::vector<size_t> shard_counts(4, 0);
	for (const auto& key : keys) {
		size_t shard_id = Shard(key, 4);
		EXPECT_TRUE(shard_id < 4);
		shard_counts[shard_id]++;
	}

	for (size_t i = 0; i < 4; ++i) {
		EXPECT_GT(shard_counts[i], 150) << "Shard " << i << " has " << shard_counts[i] << " keys";
		EXPECT_LT(shard_counts[i], 450) << "Shard " << i << " has " << shard_counts[i] << " keys";
	}
}

TEST_F(MultiShardIntegrationTest, ShardingConsistency) {
	std::string test_key = "consistency_test_key";

	size_t shard_id_4_shards = Shard(test_key, 4);
	size_t shard_id_8_shards = Shard(test_key, 8);
	size_t shard_id_16_shards = Shard(test_key, 16);

	EXPECT_TRUE(shard_id_4_shards < 4);
	EXPECT_TRUE(shard_id_8_shards < 8);
	EXPECT_TRUE(shard_id_16_shards < 16);

	EXPECT_EQ(Shard(test_key, 4), shard_id_4_shards);
	EXPECT_EQ(Shard(test_key, 8), shard_id_8_shards);
	EXPECT_EQ(Shard(test_key, 16), shard_id_16_shards);
}

TEST_F(MultiShardIntegrationTest, CrossShardMGetSimulation) {
	Database db;
	CommandContext ctx(&db, 0);

	std::vector<std::string> keys_to_set;
	std::vector<std::string> values_to_set;

	const uint32_t key_count = 10;
	keys_to_set.reserve(key_count);
	values_to_set.reserve(key_count);
	for (uint32_t key_idx = 0; key_idx < key_count; ++key_idx) {
		std::string key = "key_" + std::to_string(key_idx);
		std::string value = "value_" + std::to_string(key_idx);
		keys_to_set.emplace_back(key);
		values_to_set.emplace_back(value);
		EXPECT_EQ(ExecuteCommand("SET", {key, value}, &ctx), "+OK\r\n");
	}

	std::string response = ExecuteCommand("MGET", keys_to_set, &ctx);
	EXPECT_TRUE(response.find("*10") == 0);

	std::vector<std::string> values;
	ParseRESPArray(response, values);
	EXPECT_EQ(values.size(), 10);

	for (uint32_t key_idx = 0; key_idx < key_count; ++key_idx) {
		EXPECT_EQ(values[key_idx], values_to_set[key_idx]) << "Mismatch for key_" << key_idx;
	}
}

TEST_F(MultiShardIntegrationTest, LargeMSetMGet) {
	Database db;
	CommandContext ctx(&db, 0);

	const uint32_t num_keys = 100;
	std::vector<std::string> args;
	args.reserve(static_cast<size_t>(num_keys) * 2U);
	for (uint32_t key_idx = 0; key_idx < num_keys; ++key_idx) {
		args.emplace_back("key_" + std::to_string(key_idx));
		args.emplace_back("value_" + std::to_string(key_idx));
	}

	EXPECT_EQ(ExecuteCommand("MSET", args, &ctx), "+OK\r\n");

	args.clear();
	args.reserve(num_keys);
	for (uint32_t key_idx = 0; key_idx < num_keys; ++key_idx) {
		args.emplace_back("key_" + std::to_string(key_idx));
	}

	std::string response = ExecuteCommand("MGET", args, &ctx);
	EXPECT_TRUE(response.find("*100") == 0);

	EXPECT_EQ(ExecuteCommand("DBSIZE", {}, &ctx), ":" + std::to_string(num_keys) + "\r\n");
}
