#include <gtest/gtest.h>
#include <string>
#include <set>
#include "storage/database.h"

class DatabaseTest : public ::testing::Test {
protected:
    Database db_;
};

TEST_F(DatabaseTest, SetAndGet) {
    EXPECT_TRUE(db_.Set("key1", "value1"));
    auto result = db_.Get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
}

TEST_F(DatabaseTest, GetNonExistent) {
    auto result = db_.Get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, SetOverwrite) {
    db_.Set("key1", "value1");
    db_.Set("key1", "value2");

    auto result = db_.Get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value2");
}

TEST_F(DatabaseTest, DeleteExisting) {
    db_.Set("key1", "value1");
    EXPECT_TRUE(db_.Del("key1"));

    auto result = db_.Get("key1");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, DeleteNonExistent) {
    EXPECT_FALSE(db_.Del("nonexistent"));
}

TEST_F(DatabaseTest, Exists) {
    EXPECT_FALSE(db_.Exists("key1"));

    db_.Set("key1", "value1");
    EXPECT_TRUE(db_.Exists("key1"));

    db_.Del("key1");
    EXPECT_FALSE(db_.Exists("key1"));
}

TEST_F(DatabaseTest, KeyCount) {
    EXPECT_EQ(db_.KeyCount(), 0);

    db_.Set("key1", "value1");
    EXPECT_EQ(db_.KeyCount(), 1);

    db_.Set("key2", "value2");
    db_.Set("key3", "value3");
    EXPECT_EQ(db_.KeyCount(), 3);

    db_.Del("key1");
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
    db_.Set("key1", "value1");
    EXPECT_EQ(db_.KeyCount(), 1);

    db_.Select(1);
    EXPECT_EQ(db_.KeyCount(), 0);

    db_.Set("key2", "value2");
    EXPECT_EQ(db_.KeyCount(), 1);

    db_.Select(0);
    EXPECT_EQ(db_.KeyCount(), 1);

    auto result = db_.Get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");

    db_.Select(1);
    result = db_.Get("key2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value2");
}

TEST_F(DatabaseTest, ClearCurrentDatabase) {
    db_.Set("key1", "value1");
    db_.Set("key2", "value2");
    EXPECT_EQ(db_.KeyCount(), 2);

    db_.ClearCurrentDB();
    EXPECT_EQ(db_.KeyCount(), 0);

    auto result = db_.Get("key1");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, ClearAllDatabases) {
    db_.Set("key1", "value1");
    EXPECT_EQ(db_.KeyCount(), 1);

    db_.Select(1);
    db_.Set("key2", "value2");
    EXPECT_EQ(db_.KeyCount(), 1);

    db_.Select(2);
    db_.Set("key3", "value3");
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
        db_.Set(key, value);
    }

    EXPECT_EQ(db_.KeyCount(), N);

    for (int i = 0; i < N; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected = "value" + std::to_string(i);
        auto result = db_.Get(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expected);
    }
}

TEST_F(DatabaseTest, MixedOperations) {
    db_.Set("a", "1");
    db_.Set("b", "2");
    db_.Set("c", "3");

    auto result_a = db_.Get("a");
    ASSERT_TRUE(result_a.has_value());
    EXPECT_EQ(result_a.value(), "1");

    EXPECT_TRUE(db_.Del("b"));
    EXPECT_FALSE(db_.Exists("b"));

    db_.Set("b", "new2");
    auto result_b = db_.Get("b");
    ASSERT_TRUE(result_b.has_value());
    EXPECT_EQ(result_b.value(), "new2");

    EXPECT_EQ(db_.KeyCount(), 3);
}

TEST_F(DatabaseTest, SelectBackAndForth) {
    db_.Set("db0_key", "db0_value");

    db_.Select(1);
    db_.Set("db1_key", "db1_value");

    db_.Select(0);
    auto result = db_.Get("db0_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "db0_value");

    db_.Select(1);
    result = db_.Get("db1_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "db1_value");
}

TEST_F(DatabaseTest, Keys) {
    db_.Set("key1", "value1");
    db_.Set("key2", "value2");
    db_.Set("key3", "value3");

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
