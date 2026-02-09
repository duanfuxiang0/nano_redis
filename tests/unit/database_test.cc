#include <gtest/gtest.h>
#include <cstdint>
#include <chrono>
#include <string>
#include <set>
#include <thread>
#include "core/database.h"
#include "core/nano_obj.h"

class DatabaseTest : public ::testing::Test {
protected:
	Database db;
};

TEST_F(DatabaseTest, SetAndGet) {
	EXPECT_TRUE(db.Set(NanoObj::FromKey("key1"), "value1"));
	const auto result = db.Get(NanoObj::FromKey("key1"));
	ASSERT_EQ(result, std::optional<std::string> {"value1"});
}

TEST_F(DatabaseTest, GetNonExistent) {
	const auto result = db.Get(NanoObj::FromKey("nonexistent"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, SetOverwrite) {
	db.Set(NanoObj::FromKey("key1"), "value1");
	db.Set(NanoObj::FromKey("key1"), "value2");

	const auto result = db.Get(NanoObj::FromKey("key1"));
	ASSERT_EQ(result, std::optional<std::string> {"value2"});
}

TEST_F(DatabaseTest, DeleteExisting) {
	db.Set(NanoObj::FromKey("key1"), "value1");
	EXPECT_TRUE(db.Del(NanoObj::FromKey("key1")));

	const auto result = db.Get(NanoObj::FromKey("key1"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, DeleteNonExistent) {
	EXPECT_FALSE(db.Del(NanoObj::FromKey("nonexistent")));
}

TEST_F(DatabaseTest, Exists) {
	EXPECT_FALSE(db.Exists(NanoObj::FromKey("key1")));

	db.Set(NanoObj::FromKey("key1"), "value1");
	EXPECT_TRUE(db.Exists(NanoObj::FromKey("key1")));

	db.Del(NanoObj::FromKey("key1"));
	EXPECT_FALSE(db.Exists(NanoObj::FromKey("key1")));
}

TEST_F(DatabaseTest, KeyCount) {
	EXPECT_EQ(db.KeyCount(), 0);

	db.Set(NanoObj::FromKey("key1"), "value1");
	EXPECT_EQ(db.KeyCount(), 1);

	db.Set(NanoObj::FromKey("key2"), "value2");
	db.Set(NanoObj::FromKey("key3"), "value3");
	EXPECT_EQ(db.KeyCount(), 3);

	db.Del(NanoObj::FromKey("key1"));
	EXPECT_EQ(db.KeyCount(), 2);
}

TEST_F(DatabaseTest, SelectValidDatabase) {
	EXPECT_EQ(db.CurrentDB(), 0);

	EXPECT_TRUE(db.Select(5));
	EXPECT_EQ(db.CurrentDB(), 5);

	EXPECT_TRUE(db.Select(15));
	EXPECT_EQ(db.CurrentDB(), 15);
}

TEST_F(DatabaseTest, SelectInvalidDatabase) {
	EXPECT_EQ(db.CurrentDB(), 0);

	EXPECT_FALSE(db.Select(16));
	EXPECT_EQ(db.CurrentDB(), 0);

	EXPECT_FALSE(db.Select(100));
	EXPECT_EQ(db.CurrentDB(), 0);
}

TEST_F(DatabaseTest, MultipleDatabasesIsolation) {
	db.Set(NanoObj::FromKey("key1"), "value1");
	EXPECT_EQ(db.KeyCount(), 1);

	db.Select(1);
	EXPECT_EQ(db.KeyCount(), 0);

	db.Set(NanoObj::FromKey("key2"), "value2");
	EXPECT_EQ(db.KeyCount(), 1);

	db.Select(0);
	EXPECT_EQ(db.KeyCount(), 1);

	auto result = db.Get(NanoObj::FromKey("key1"));
	ASSERT_EQ(result, std::optional<std::string> {"value1"});

	db.Select(1);
	result = db.Get(NanoObj::FromKey("key2"));
	ASSERT_EQ(result, std::optional<std::string> {"value2"});
}

TEST_F(DatabaseTest, ClearCurrentDatabase) {
	db.Set(NanoObj::FromKey("key1"), "value1");
	db.Set(NanoObj::FromKey("key2"), "value2");
	EXPECT_EQ(db.KeyCount(), 2);

	db.ClearCurrentDB();
	EXPECT_EQ(db.KeyCount(), 0);

	const auto result = db.Get(NanoObj::FromKey("key1"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, ClearAllDatabases) {
	db.Set(NanoObj::FromKey("key1"), "value1");
	EXPECT_EQ(db.KeyCount(), 1);

	db.Select(1);
	db.Set(NanoObj::FromKey("key2"), "value2");
	EXPECT_EQ(db.KeyCount(), 1);

	db.Select(2);
	db.Set(NanoObj::FromKey("key3"), "value3");
	EXPECT_EQ(db.KeyCount(), 1);

	db.ClearAll();

	for (size_t db_index = 0; db_index < Database::kNumDBs; ++db_index) {
		db.Select(db_index);
		EXPECT_EQ(db.KeyCount(), 0);
	}
}

TEST_F(DatabaseTest, BulkInsert) {
	const uint32_t n = 100;
	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		std::string key = "key" + std::to_string(key_idx);
		std::string value = "value" + std::to_string(key_idx);
		db.Set(NanoObj::FromKey(key), value);
	}

	EXPECT_EQ(db.KeyCount(), n);

	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		std::string key_str = "key" + std::to_string(key_idx);
		std::string expected = "value" + std::to_string(key_idx);
		const auto result = db.Get(NanoObj::FromKey(key_str));
		ASSERT_EQ(result, std::optional<std::string> {expected});
	}
}

TEST_F(DatabaseTest, MixedOperations) {
	db.Set(NanoObj::FromKey("a"), "1");
	db.Set(NanoObj::FromKey("b"), "2");
	db.Set(NanoObj::FromKey("c"), "3");

	const auto result_a = db.Get(NanoObj::FromKey("a"));
	ASSERT_EQ(result_a, std::optional<std::string> {"1"});

	EXPECT_TRUE(db.Del(NanoObj::FromKey("b")));
	EXPECT_FALSE(db.Exists(NanoObj::FromKey("b")));

	db.Set(NanoObj::FromKey("b"), "new2");
	const auto result_b = db.Get(NanoObj::FromKey("b"));
	ASSERT_EQ(result_b, std::optional<std::string> {"new2"});

	EXPECT_EQ(db.KeyCount(), 3);
}

TEST_F(DatabaseTest, SelectBackAndForth) {
	db.Set(NanoObj::FromKey("db0_key"), "db0_value");

	db.Select(1);
	db.Set(NanoObj::FromKey("db1_key"), "db1_value");

	db.Select(0);
	auto result = db.Get(NanoObj::FromKey("db0_key"));
	ASSERT_EQ(result, std::optional<std::string> {"db0_value"});

	db.Select(1);
	result = db.Get(NanoObj::FromKey("db1_key"));
	ASSERT_EQ(result, std::optional<std::string> {"db1_value"});
}

TEST_F(DatabaseTest, Keys) {
	db.Set(NanoObj::FromKey("key1"), "value1");
	db.Set(NanoObj::FromKey("key2"), "value2");
	db.Set(NanoObj::FromKey("key3"), "value3");

	const auto keys = db.Keys();
	EXPECT_EQ(keys.size(), 3);

	std::set<std::string> key_set(keys.begin(), keys.end());
	EXPECT_TRUE(key_set.count("key1"));
	EXPECT_TRUE(key_set.count("key2"));
	EXPECT_TRUE(key_set.count("key3"));
}

TEST_F(DatabaseTest, KeysEmpty) {
	const auto keys = db.Keys();
	EXPECT_EQ(keys.size(), 0);
}

TEST_F(DatabaseTest, ExpireAndTTL) {
	const NanoObj key = NanoObj::FromKey("ttl_key");
	db.Set(key, "value");

	EXPECT_TRUE(db.Expire(key, 2000));
	int64_t ttl = db.TTL(key);
	EXPECT_GE(ttl, 0);
	EXPECT_LE(ttl, 2);
}

TEST_F(DatabaseTest, PersistRemovesTTL) {
	const NanoObj key = NanoObj::FromKey("persist_key");
	db.Set(key, "value");
	EXPECT_TRUE(db.Expire(key, 5000));
	EXPECT_GE(db.TTL(key), 0);

	EXPECT_TRUE(db.Persist(key));
	EXPECT_EQ(db.TTL(key), -1);
	EXPECT_FALSE(db.Persist(key));
}

TEST_F(DatabaseTest, ExpireZeroDeletesKey) {
	const NanoObj key = NanoObj::FromKey("delete_key");
	db.Set(key, "value");

	EXPECT_TRUE(db.Expire(key, 0));
	EXPECT_FALSE(db.Exists(key));
	EXPECT_EQ(db.TTL(key), -2);
}

TEST_F(DatabaseTest, ActiveExpireCycleRemovesExpiredKeys) {
	const NanoObj key0 = NanoObj::FromKey("db0_key");
	const NanoObj key1 = NanoObj::FromKey("db1_key");

	db.Select(0);
	db.Set(key0, "v0");
	EXPECT_TRUE(db.Expire(key0, 1));

	db.Select(1);
	db.Set(key1, "v1");
	EXPECT_TRUE(db.Expire(key1, 1));

	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	EXPECT_GE(db.ActiveExpireCycle(64), 2U);

	db.Select(0);
	EXPECT_FALSE(db.Exists(key0));
	db.Select(1);
	EXPECT_FALSE(db.Exists(key1));
}
