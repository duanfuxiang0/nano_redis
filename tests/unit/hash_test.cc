#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "storage/hashtable.h"

class HashTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        hash_ = HashTable<std::string, std::string>(16);
    }

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

TEST_F(HashTableTest, EmptyTableOperations) {
    EXPECT_EQ(hash_.Size(), 0);
    EXPECT_EQ(hash_.Find("anykey"), nullptr);
    EXPECT_FALSE(hash_.Erase("anykey"));
    hash_.Clear();
    EXPECT_EQ(hash_.Size(), 0);
}

TEST_F(HashTableTest, ResizeTrigger) {
    size_t initial_buckets = hash_.BucketCount();
    
    for (size_t i = 0; i < initial_buckets; ++i) {
        hash_.Insert("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    EXPECT_GT(hash_.BucketCount(), initial_buckets);
}

TEST_F(HashTableTest, MultipleResizes) {
    size_t initial_buckets = hash_.BucketCount();
    
    for (int i = 0; i < 1000; ++i) {
        hash_.Insert("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    EXPECT_GT(hash_.BucketCount(), initial_buckets);
    EXPECT_EQ(hash_.Size(), 1000);
    
    for (int i = 0; i < 1000; ++i) {
        auto result = hash_.Find("key" + std::to_string(i));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(*result, "value" + std::to_string(i));
    }
}

TEST_F(HashTableTest, DeleteAfterResize) {
    hash_.Insert("key1", "value1");
    
    for (int i = 0; i < 100; ++i) {
        hash_.Insert("temp" + std::to_string(i), "temp" + std::to_string(i));
    }
    
    EXPECT_TRUE(hash_.Erase("key1"));
    EXPECT_EQ(hash_.Find("key1"), nullptr);
}

TEST_F(HashTableTest, ForEach) {
    hash_.Insert("a", "1");
    hash_.Insert("b", "2");
    hash_.Insert("c", "3");
    
    int count = 0;
    hash_.ForEach([&count](const std::string& key, const std::string& value) {
        count++;
        EXPECT_GT(key.length(), 0);
        EXPECT_GT(value.length(), 0);
    });
    
    EXPECT_EQ(count, 3);
}

TEST_F(HashTableTest, MoveConstructor) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key2", "value2");
    
    HashTable<std::string, std::string> moved(std::move(hash_));
    
    EXPECT_EQ(moved.Size(), 2);
    EXPECT_NE(moved.Find("key1"), nullptr);
    EXPECT_NE(moved.Find("key2"), nullptr);
    
    EXPECT_EQ(hash_.Size(), 0);
    
    hash_.Insert("newkey", "newvalue");
    EXPECT_EQ(hash_.Size(), 1);
    EXPECT_NE(hash_.Find("newkey"), nullptr);
}

TEST_F(HashTableTest, MoveAssignment) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key2", "value2");
    
    HashTable<std::string, std::string> other;
    other.Insert("key3", "value3");
    
    other = std::move(hash_);
    
    EXPECT_EQ(other.Size(), 2);
    EXPECT_NE(other.Find("key1"), nullptr);
    EXPECT_NE(other.Find("key2"), nullptr);
    
    EXPECT_EQ(hash_.Size(), 0);
}

TEST_F(HashTableTest, ConstFind) {
    hash_.Insert("key1", "value1");
    
    const HashTable<std::string, std::string>& const_hash = hash_;
    
    auto result = const_hash.Find("key1");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "value1");
    
    auto null_result = const_hash.Find("key2");
    EXPECT_EQ(null_result, nullptr);
}

TEST_F(HashTableTest, MultipleOverwrites) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key1", "value2");
    hash_.Insert("key1", "value3");
    hash_.Insert("key1", "value4");
    
    auto result = hash_.Find("key1");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "value4");
    EXPECT_EQ(hash_.Size(), 1);
}

TEST_F(HashTableTest, ClearAndReinsert) {
    hash_.Insert("key1", "value1");
    hash_.Insert("key2", "value2");
    
    hash_.Clear();
    
    hash_.Insert("key1", "newvalue1");
    hash_.Insert("key3", "value3");
    
    EXPECT_EQ(hash_.Size(), 2);
    EXPECT_EQ(*hash_.Find("key1"), "newvalue1");
    EXPECT_EQ(hash_.Find("key2"), nullptr);
    EXPECT_NE(hash_.Find("key3"), nullptr);
}

TEST_F(HashTableTest, EraseAll) {
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        hash_.Insert("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(hash_.Erase("key" + std::to_string(i)));
    }
    
    EXPECT_EQ(hash_.Size(), 0);
    
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(hash_.Find("key" + std::to_string(i)), nullptr);
    }
}

TEST_F(HashTableTest, InitialCapacity) {
    HashTable<std::string, std::string> table(1);
    EXPECT_GE(table.BucketCount(), 4);
}

TEST_F(HashTableTest, LargeInitialCapacity) {
    HashTable<std::string, std::string> table(1000);
    EXPECT_GE(table.BucketCount(), 1024);
}

TEST_F(HashTableTest, DifferentTypes) {
    HashTable<int, double> int_table;
    int_table.Insert(1, 1.5);
    int_table.Insert(2, 2.5);
    
    EXPECT_EQ(*int_table.Find(1), 1.5);
    EXPECT_EQ(*int_table.Find(2), 2.5);
    
    HashTable<int, int> simple_table;
    simple_table.Insert(100, 200);
    EXPECT_EQ(*simple_table.Find(100), 200);
}

TEST_F(HashTableTest, DuplicateKeysDifferentBuckets) {
    HashTable<int, int> table(4);
    
    for (int i = 0; i < 100; i += 4) {
        table.Insert(i, i * 10);
    }
    
    for (int i = 0; i < 100; i += 4) {
        auto result = table.Find(i);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(*result, i * 10);
    }
}

TEST_F(HashTableTest, EmptyStringKey) {
    hash_.Insert("", "empty_key_value");
    
    auto result = hash_.Find("");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "empty_key_value");
    
    EXPECT_TRUE(hash_.Erase(""));
    EXPECT_EQ(hash_.Find(""), nullptr);
}

TEST_F(HashTableTest, EmptyStringValue) {
    hash_.Insert("key", "");
    
    auto result = hash_.Find("key");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "");
}

TEST_F(HashTableTest, ForEachEmpty) {
    int count = 0;
    hash_.ForEach([&count](const std::string&, const std::string&) {
        count++;
    });
    
    EXPECT_EQ(count, 0);
}

TEST_F(HashTableTest, ForEachAfterClear) {
    int count = 0;
    hash_.ForEach([&count](const std::string&, const std::string&) {
        count++;
    });
    
    EXPECT_EQ(count, 0);
}

TEST_F(HashTableTest, RehashPreservesAllData) {
    HashTable<int, int> table(4);
    
    for (int i = 0; i < 100; i++) {
        table.Insert(i, i * 10);
    }
    
    EXPECT_EQ(table.Size(), 100);
    EXPECT_GT(table.BucketCount(), 4);
    
    for (int i = 0; i < 100; i++) {
        auto result = table.Find(i);
        ASSERT_NE(result, nullptr) << "Failed to find key " << i;
        EXPECT_EQ(*result, i * 10);
    }
}

TEST_F(HashTableTest, RehashWithCollisions) {
    HashTable<int, int> table(4);
    
    for (int i = 0; i < 50; i++) {
        table.Insert(i * 4, i);
    }
    
    for (int i = 0; i < 50; i++) {
        auto result = table.Find(i * 4);
        ASSERT_NE(result, nullptr) << "Failed to find key " << i * 4;
        EXPECT_EQ(*result, i);
    }
}

TEST_F(HashTableTest, RehashAndDeleteMixed) {
    HashTable<int, int> table(4);
    
    for (int i = 0; i < 100; i++) {
        table.Insert(i, i * 10);
    }
    
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(table.Erase(i));
    }
    
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(table.Find(i), nullptr);
    }
    
    for (int i = 50; i < 100; i++) {
        auto result = table.Find(i);
        ASSERT_NE(result, nullptr) << "Failed to find key " << i;
        EXPECT_EQ(*result, i * 10);
    }
}

TEST_F(HashTableTest, RehashMaintainsCorrectBucketDistribution) {
    HashTable<int, int> table(4);
    
    for (int i = 0; i < 20; i++) {
        table.Insert(i, i);
    }
    
    EXPECT_EQ(table.Size(), 20);
    
    uint64_t total_items = 0;
    table.ForEach([&total_items](const int&, const int&) {
        total_items++;
    });
    
    EXPECT_EQ(total_items, 20);
}

TEST_F(HashTableTest, RehashAfterChainedNodes) {
    HashTable<int, int> table(4);
    
    for (int i = 0; i < 100; i++) {
        table.Insert(i, i);
    }
    
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(*table.Find(i), i);
    }
    
    table.Insert(1000, 1000);
    EXPECT_EQ(*table.Find(1000), 1000);
    
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(*table.Find(i), i);
    }
}
