#include <gtest/gtest.h>
#include <string>
#include "storage/hashtable.h"

class HashTableTest : public ::testing::Test {
protected:
    HashTable<std::string, std::string> hash_{16};
};

TEST_F(HashTableTest, InsertAndFind) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key2", "value2");

    auto result1 = hash_.Find("key1");
    ASSERT_NE(result1, nullptr);
    EXPECT_EQ(*result1, "value1");

    auto result2 = hash_.Find("key2");
    ASSERT_NE(result2, nullptr);
    EXPECT_EQ(*result2, "value2");
}

TEST_F(HashTableTest, InsertOverwrite) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key1", "value2");

    auto result = hash_.Find("key1");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "value2");
}

TEST_F(HashTableTest, FindNonExistent) {
    hash_.Insert("key1", "value1");
    auto result = hash_.Find("key2");
    EXPECT_EQ(result, nullptr);
}

TEST_F(HashTableTest, EraseExisting) {
    hash_.Insert("key1", "value1");
    EXPECT_TRUE(hash_.Erase("key1"));

    auto result = hash_.Find("key1");
    EXPECT_EQ(result, nullptr);
}

TEST_F(HashTableTest, EraseNonExistent) {
    hash_.Insert("key1", "value1");
    EXPECT_FALSE(hash_.Erase("key2"));
}

TEST_F(HashTableTest, Size) {
    EXPECT_EQ(hash_.Size(), 0);
    hash_.Insert("key1", "value1");
    EXPECT_EQ(hash_.Size(), 1);
    hash_.Insert("key2", "value2");
    EXPECT_EQ(hash_.Size(), 2);
    hash_.Erase("key1");
    EXPECT_EQ(hash_.Size(), 1);
}

TEST_F(HashTableTest, Clear) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key2", "value2");
    hash_.Insert("key3", "value3");
    EXPECT_GT(hash_.Size(), 0);

    hash_.Clear();
    EXPECT_EQ(hash_.Size(), 0);

    auto result = hash_.Find("key1");
    EXPECT_EQ(result, nullptr);
}

TEST_F(HashTableTest, BucketCount) {
    size_t bucket_count = hash_.BucketCount();
    EXPECT_GT(bucket_count, 0);
}

TEST_F(HashTableTest, BulkInsert) {
    const int N = 1000;
    for (int i = 0; i < N; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        hash_.Insert(key, value);
    }

    EXPECT_EQ(hash_.Size(), N);

    for (int i = 0; i < N; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected = "value" + std::to_string(i);
        auto result = hash_.Find(key);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(*result, expected);
    }
}

TEST_F(HashTableTest, MixedOperations) {
    hash_.Insert("a", "1");
    hash_.Insert("b", "2");
    hash_.Insert("c", "3");

    auto result_a = hash_.Find("a");
    ASSERT_NE(result_a, nullptr);
    EXPECT_EQ(*result_a, "1");
    EXPECT_TRUE(hash_.Erase("b"));
    EXPECT_EQ(hash_.Find("b"), nullptr);
    hash_.Insert("b", "new2");
    auto result_b = hash_.Find("b");
    ASSERT_NE(result_b, nullptr);
    EXPECT_EQ(*result_b, "new2");
    EXPECT_EQ(hash_.Size(), 3);
}
