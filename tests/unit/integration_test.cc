#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "command/string_family.h"
#include "command/command_registry.h"
#include "core/compact_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "server/engine_shard_set.h"
#include "server/engine_shard.h"
#include "server/sharding.h"
#include "protocol/resp_parser.h"
#include <vector>
#include <string>

class MultiShardIntegrationTest : public ::testing::Test {
protected:
	void SetUp() override {
		registry_ = &CommandRegistry::instance();
		StringFamily::Register(registry_);
	}

	std::string ExecuteCommand(const std::string& cmd, const std::vector<std::string>& args, CommandContext* ctx) {
		std::vector<CompactObj> full_args = {CompactObj::fromKey(cmd)};
		for (const auto& arg : args) {
			full_args.push_back(CompactObj::fromKey(arg));
		}
		return registry_->execute(full_args, ctx);
	}

	void ParseRESPArray(const std::string& resp, std::vector<std::string>& values) {
		values.clear();
		size_t pos = resp.find('\n', 0);
		if (pos == std::string::npos) return;

		size_t count = 0;
		try {
			size_t star_pos = resp.find('*');
			if (star_pos != std::string::npos) {
				count = std::stoul(resp.substr(star_pos + 1));
			}
		} catch (...) {
			return;
		}

		pos++;
		for (size_t i = 0; i < count && pos < resp.size(); ++i) {
			if (resp[pos] == '\n') pos++;
			if (resp[pos] == '$') {
				size_t dollar_pos = pos;
				size_t newline_pos = resp.find('\n', dollar_pos);
				if (newline_pos == std::string::npos) break;

				size_t len_pos = dollar_pos + 1;
				try {
			size_t len = std::stoul(resp.substr(len_pos, newline_pos - len_pos));
			pos = newline_pos + 1;
			if (len == (size_t)-1) {
				values.push_back("(nil)");
			} else if (pos + len < resp.size()) {
				values.push_back(resp.substr(pos, len));
				pos += len + 2;
			}
				} catch (...) {
					break;
				}
			} else if (resp[pos] == '-') {
				values.push_back("(nil)");
				pos = resp.find('\n', pos) + 1;
			}
		}
	}

	CommandRegistry* registry_;
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
	std::vector<std::string> keys;
	for (int i = 0; i < 1000; ++i) {
		keys.push_back("key_" + std::to_string(i));
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

	for (int i = 0; i < 10; ++i) {
		std::string key = "key_" + std::to_string(i);
		std::string value = "value_" + std::to_string(i);
		keys_to_set.push_back(key);
		values_to_set.push_back(value);
		EXPECT_EQ(ExecuteCommand("SET", {key, value}, &ctx), "+OK\r\n");
	}

	std::string response = ExecuteCommand("MGET", keys_to_set, &ctx);
	EXPECT_TRUE(response.find("*10") == 0);

	std::vector<std::string> values;
	ParseRESPArray(response, values);
	EXPECT_EQ(values.size(), 10);

	for (int i = 0; i < 10; ++i) {
		EXPECT_EQ(values[i], values_to_set[i]) << "Mismatch for key_" << i;
	}
}

TEST_F(MultiShardIntegrationTest, LargeMSetMGet) {
	Database db;
	CommandContext ctx(&db, 0);

	const int kNumKeys = 100;
	std::vector<std::string> args;
	for (int i = 0; i < kNumKeys; ++i) {
		args.push_back("key_" + std::to_string(i));
		args.push_back("value_" + std::to_string(i));
	}

	EXPECT_EQ(ExecuteCommand("MSET", args, &ctx), "+OK\r\n");

	args.clear();
	for (int i = 0; i < kNumKeys; ++i) {
		args.push_back("key_" + std::to_string(i));
	}

	std::string response = ExecuteCommand("MGET", args, &ctx);
	EXPECT_TRUE(response.find("*100") == 0);

	EXPECT_EQ(ExecuteCommand("DBSIZE", {}, &ctx), ":" + std::to_string(kNumKeys) + "\r\n");
}
