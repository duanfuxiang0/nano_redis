#include <gtest/gtest.h>
#include <string>
#include <set>
#include "core/database.h"
#include "core/nano_obj.h"
#include "core/nano_obj.h"

class DatabaseTest : public ::testing::Test {
protected:
	Database db_;
};

TEST_F(DatabaseTest, SetAndGet) {
	EXPECT_TRUE(db_.Set(NanoObj::fromKey("key1"), "value1"));
	auto result = db_.Get(NanoObj::fromKey("key1"));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "value1");
}

TEST_F(DatabaseTest, GetNonExistent) {
	auto result = db_.Get(NanoObj::fromKey("nonexistent"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, SetOverwrite) {
	db_.Set(NanoObj::fromKey("key1"), "value1");
	db_.Set(NanoObj::fromKey("key1"), "value2");

	auto result = db_.Get(NanoObj::fromKey("key1"));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "value2");
}

TEST_F(DatabaseTest, DeleteExisting) {
	db_.Set(NanoObj::fromKey("key1"), "value1");
	EXPECT_TRUE(db_.Del(NanoObj::fromKey("key1")));

	auto result = db_.Get(NanoObj::fromKey("key1"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, DeleteNonExistent) {
	EXPECT_FALSE(db_.Del(NanoObj::fromKey("nonexistent")));
}

TEST_F(DatabaseTest, Exists) {
	EXPECT_FALSE(db_.Exists(NanoObj::fromKey("key1")));

	db_.Set(NanoObj::fromKey("key1"), "value1");
	EXPECT_TRUE(db_.Exists(NanoObj::fromKey("key1")));

	db_.Del(NanoObj::fromKey("key1"));
	EXPECT_FALSE(db_.Exists(NanoObj::fromKey("key1")));
}

TEST_F(DatabaseTest, KeyCount) {
	EXPECT_EQ(db_.KeyCount(), 0);

	db_.Set(NanoObj::fromKey("key1"), "value1");
	EXPECT_EQ(db_.KeyCount(), 1);

	db_.Set(NanoObj::fromKey("key2"), "value2");
	db_.Set(NanoObj::fromKey("key3"), "value3");
	EXPECT_EQ(db_.KeyCount(), 3);

	db_.Del(NanoObj::fromKey("key1"));
	EXPECT_EQ(db_.KeyCount(), 2);
}

TEST_F(DatabaseTest, SelectValidDatabase) {
	EXPECT_EQ(db_.CurrentDB(), 0);

	EXPECT_TRUE(db_.Select(5));
	EXPECT_EQ(db_.CurrentDB(), 5);

	EXPECT_TRUE(db_.Select(15));
	EXPECT_EQ(db_.CurrentDB(), 15);
}

TEST_F(DatabaseTest, SelectInvalidDatabase) {
	EXPECT_EQ(db_.CurrentDB(), 0);

	EXPECT_FALSE(db_.Select(16));
	EXPECT_EQ(db_.CurrentDB(), 0);

	EXPECT_FALSE(db_.Select(100));
	EXPECT_EQ(db_.CurrentDB(), 0);
}

TEST_F(DatabaseTest, MultipleDatabasesIsolation) {
	db_.Set(NanoObj::fromKey("key1"), "value1");
	EXPECT_EQ(db_.KeyCount(), 1);

	db_.Select(1);
	EXPECT_EQ(db_.KeyCount(), 0);

	db_.Set(NanoObj::fromKey("key2"), "value2");
	EXPECT_EQ(db_.KeyCount(), 1);

	db_.Select(0);
	EXPECT_EQ(db_.KeyCount(), 1);

	auto result = db_.Get(NanoObj::fromKey("key1"));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "value1");

	db_.Select(1);
	result = db_.Get(NanoObj::fromKey("key2"));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "value2");
}

TEST_F(DatabaseTest, ClearCurrentDatabase) {
	db_.Set(NanoObj::fromKey("key1"), "value1");
	db_.Set(NanoObj::fromKey("key2"), "value2");
	EXPECT_EQ(db_.KeyCount(), 2);

	db_.ClearCurrentDB();
	EXPECT_EQ(db_.KeyCount(), 0);

	auto result = db_.Get(NanoObj::fromKey("key1"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, ClearAllDatabases) {
	db_.Set(NanoObj::fromKey("key1"), "value1");
	EXPECT_EQ(db_.KeyCount(), 1);

	db_.Select(1);
	db_.Set(NanoObj::fromKey("key2"), "value2");
	EXPECT_EQ(db_.KeyCount(), 1);

	db_.Select(2);
	db_.Set(NanoObj::fromKey("key3"), "value3");
	EXPECT_EQ(db_.KeyCount(), 1);

	db_.ClearAll();

	for (size_t i = 0; i < Database::kNumDBs; ++i) {
		db_.Select(i);
		EXPECT_EQ(db_.KeyCount(), 0);
	}
}

TEST_F(DatabaseTest, BulkInsert) {
	const int N = 100;
	for (int i = 0; i < N; ++i) {
		std::string key = "key" + std::to_string(i);
		std::string value = "value" + std::to_string(i);
		db_.Set(NanoObj::fromKey(key), value);
	}

	EXPECT_EQ(db_.KeyCount(), N);

	for (int i = 0; i < N; ++i) {
		std::string key_str = "key" + std::to_string(i);
		std::string expected = "value" + std::to_string(i);
		auto result = db_.Get(NanoObj::fromKey(key_str));
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result.value(), expected);
	}
}

TEST_F(DatabaseTest, MixedOperations) {
	db_.Set(NanoObj::fromKey("a"), "1");
	db_.Set(NanoObj::fromKey("b"), "2");
	db_.Set(NanoObj::fromKey("c"), "3");

	auto result_a = db_.Get(NanoObj::fromKey("a"));
	ASSERT_TRUE(result_a.has_value());
	EXPECT_EQ(result_a.value(), "1");

	EXPECT_TRUE(db_.Del(NanoObj::fromKey("b")));
	EXPECT_FALSE(db_.Exists(NanoObj::fromKey("b")));

	db_.Set(NanoObj::fromKey("b"), "new2");
	auto result_b = db_.Get(NanoObj::fromKey("b"));
	ASSERT_TRUE(result_b.has_value());
	EXPECT_EQ(result_b.value(), "new2");

	EXPECT_EQ(db_.KeyCount(), 3);
}

TEST_F(DatabaseTest, SelectBackAndForth) {
	db_.Set(NanoObj::fromKey("db0_key"), "db0_value");

	db_.Select(1);
	db_.Set(NanoObj::fromKey("db1_key"), "db1_value");

	db_.Select(0);
	auto result = db_.Get(NanoObj::fromKey("db0_key"));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "db0_value");

	db_.Select(1);
	result = db_.Get(NanoObj::fromKey("db1_key"));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "db1_value");
}

TEST_F(DatabaseTest, Keys) {
	db_.Set(NanoObj::fromKey("key1"), "value1");
	db_.Set(NanoObj::fromKey("key2"), "value2");
	db_.Set(NanoObj::fromKey("key3"), "value3");

	auto keys = db_.Keys();
	EXPECT_EQ(keys.size(), 3);

	std::set<std::string> key_set(keys.begin(), keys.end());
	EXPECT_TRUE(key_set.count("key1"));
	EXPECT_TRUE(key_set.count("key2"));
	EXPECT_TRUE(key_set.count("key3"));
}

TEST_F(DatabaseTest, KeysEmpty) {
	auto keys = db_.Keys();
	EXPECT_EQ(keys.size(), 0);
}
