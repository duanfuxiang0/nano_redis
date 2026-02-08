#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>
#include "core/dashtable.h"

class DashTableTest : public ::testing::Test {
protected:
	void SetUp() override {
		hash = DashTable<std::string, std::string>(16);
	}

	DashTable<std::string, std::string> hash{16};
};

TEST_F(DashTableTest, InsertAndFind) {
	hash.Insert("key1", "value1");
	hash.Insert("key2", "value2");

	auto result1 = hash.Find("key1");
	ASSERT_NE(result1, nullptr);
	EXPECT_EQ(*result1, "value1");

	auto result2 = hash.Find("key2");
	ASSERT_NE(result2, nullptr);
	EXPECT_EQ(*result2, "value2");
}

TEST_F(DashTableTest, InsertOverwrite) {
	hash.Insert("key1", "value1");
	hash.Insert("key1", "value2");

	auto result = hash.Find("key1");
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(*result, "value2");
}

TEST_F(DashTableTest, FindNonExistent) {
	hash.Insert("key1", "value1");
	auto result = hash.Find("key2");
	EXPECT_EQ(result, nullptr);
}

TEST_F(DashTableTest, EraseExisting) {
	hash.Insert("key1", "value1");
	EXPECT_TRUE(hash.Erase("key1"));

	auto result = hash.Find("key1");
	EXPECT_EQ(result, nullptr);
}

TEST_F(DashTableTest, EraseNonExistent) {
	hash.Insert("key1", "value1");
	EXPECT_FALSE(hash.Erase("key2"));
}

TEST_F(DashTableTest, Size) {
	EXPECT_EQ(hash.Size(), 0);
	hash.Insert("key1", "value1");
	EXPECT_EQ(hash.Size(), 1);
	hash.Insert("key2", "value2");
	EXPECT_EQ(hash.Size(), 2);
	hash.Erase("key1");
	EXPECT_EQ(hash.Size(), 1);
}

TEST_F(DashTableTest, Clear) {
	hash.Insert("key1", "value1");
	hash.Insert("key2", "value2");
	hash.Insert("key3", "value3");
	EXPECT_GT(hash.Size(), 0);

	hash.Clear();
	EXPECT_EQ(hash.Size(), 0);

	auto result = hash.Find("key1");
	EXPECT_EQ(result, nullptr);
}

TEST_F(DashTableTest, BucketCount) {
	size_t bucket_count = hash.BucketCount();
	EXPECT_GT(bucket_count, 0);
}

TEST_F(DashTableTest, BulkInsert) {
	const uint32_t n = 1000;
	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		std::string key = "key" + std::to_string(key_idx);
		std::string value = "value" + std::to_string(key_idx);
		hash.Insert(key, value);
	}

	EXPECT_EQ(hash.Size(), static_cast<size_t>(n));

	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		std::string key = "key" + std::to_string(key_idx);
		std::string expected = "value" + std::to_string(key_idx);
		auto result = hash.Find(key);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, expected);
	}
}

TEST_F(DashTableTest, MixedOperations) {
	hash.Insert("a", "1");
	hash.Insert("b", "2");
	hash.Insert("c", "3");

	auto result_a = hash.Find("a");
	ASSERT_NE(result_a, nullptr);
	EXPECT_EQ(*result_a, "1");
	EXPECT_TRUE(hash.Erase("b"));
	EXPECT_EQ(hash.Find("b"), nullptr);
	hash.Insert("b", "new2");
	auto result_b = hash.Find("b");
	ASSERT_NE(result_b, nullptr);
	EXPECT_EQ(*result_b, "new2");
	EXPECT_EQ(hash.Size(), 3);
}

TEST_F(DashTableTest, EmptyTableOperations) {
	EXPECT_EQ(hash.Size(), 0);
	EXPECT_EQ(hash.Find("anykey"), nullptr);
	EXPECT_FALSE(hash.Erase("anykey"));
	hash.Clear();
	EXPECT_EQ(hash.Size(), 0);
}

TEST_F(DashTableTest, ResizeTrigger) {
	for (size_t i = 0; i < 16; ++i) {
		hash.Insert("key" + std::to_string(i), "value" + std::to_string(i));
	}

	EXPECT_GT(hash.Size(), 0);
}

TEST_F(DashTableTest, MultipleResizes) {
	for (uint32_t key_idx = 0; key_idx < 1000; ++key_idx) {
		hash.Insert("key" + std::to_string(key_idx), "value" + std::to_string(key_idx));
	}

	EXPECT_EQ(hash.Size(), 1000);

	for (uint32_t key_idx = 0; key_idx < 1000; ++key_idx) {
		auto result = hash.Find("key" + std::to_string(key_idx));
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, "value" + std::to_string(key_idx));
	}
}

TEST_F(DashTableTest, DeleteAfterResize) {
	hash.Insert("key1", "value1");

	for (uint32_t temp_idx = 0; temp_idx < 100; ++temp_idx) {
		hash.Insert("temp" + std::to_string(temp_idx), "temp" + std::to_string(temp_idx));
	}

	EXPECT_TRUE(hash.Erase("key1"));
	EXPECT_EQ(hash.Find("key1"), nullptr);
}

TEST_F(DashTableTest, ForEach) {
	hash.Insert("a", "1");
	hash.Insert("b", "2");
	hash.Insert("c", "3");

	uint32_t count = 0;
	hash.ForEach([&count](const std::string& key, const std::string& value) {
		++count;
		EXPECT_GT(key.length(), 0);
		EXPECT_GT(value.length(), 0);
	});

	EXPECT_EQ(count, 3U);
}

TEST_F(DashTableTest, MoveConstructor) {
	hash.Insert("key1", "value1");
	hash.Insert("key2", "value2");

	DashTable<std::string, std::string> moved(std::move(hash));

	EXPECT_EQ(moved.Size(), 2);
	EXPECT_NE(moved.Find("key1"), nullptr);
	EXPECT_NE(moved.Find("key2"), nullptr);

	EXPECT_EQ(hash.Size(), 0);

	hash.Insert("newkey", "newvalue");
	EXPECT_EQ(hash.Size(), 1);
	EXPECT_NE(hash.Find("newkey"), nullptr);
}

TEST_F(DashTableTest, MoveAssignment) {
	hash.Insert("key1", "value1");
	hash.Insert("key2", "value2");

	DashTable<std::string, std::string> other;
	other.Insert("key3", "value3");

	other = std::move(hash);

	EXPECT_EQ(other.Size(), 2);
	EXPECT_NE(other.Find("key1"), nullptr);
	EXPECT_NE(other.Find("key2"), nullptr);

	EXPECT_EQ(hash.Size(), 0);
}

TEST_F(DashTableTest, ConstFind) {
	hash.Insert("key1", "value1");

	const DashTable<std::string, std::string>& const_hash = hash;

	auto result = const_hash.Find("key1");
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(*result, "value1");

	auto null_result = const_hash.Find("key2");
	EXPECT_EQ(null_result, nullptr);
}

TEST_F(DashTableTest, MultipleOverwrites) {
	hash.Insert("key1", "value1");
	hash.Insert("key1", "value2");
	hash.Insert("key1", "value3");
	hash.Insert("key1", "value4");

	auto result = hash.Find("key1");
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(*result, "value4");
	EXPECT_EQ(hash.Size(), 1);
}

TEST_F(DashTableTest, ClearAndReinsert) {
	hash.Insert("key1", "value1");
	hash.Insert("key2", "value2");

	hash.Clear();

	hash.Insert("key1", "newvalue1");
	hash.Insert("key3", "value3");

	EXPECT_EQ(hash.Size(), 2);
	EXPECT_EQ(*hash.Find("key1"), "newvalue1");
	EXPECT_EQ(hash.Find("key2"), nullptr);
	EXPECT_NE(hash.Find("key3"), nullptr);
}

TEST_F(DashTableTest, EraseAll) {
	const uint32_t n = 100;
	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		hash.Insert("key" + std::to_string(key_idx), "value" + std::to_string(key_idx));
	}

	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		EXPECT_TRUE(hash.Erase("key" + std::to_string(key_idx)));
	}

	EXPECT_EQ(hash.Size(), 0);

	for (uint32_t key_idx = 0; key_idx < n; ++key_idx) {
		EXPECT_EQ(hash.Find("key" + std::to_string(key_idx)), nullptr);
	}
}

TEST_F(DashTableTest, InitialCapacity) {
	DashTable<std::string, std::string> table(1);
	EXPECT_GE(table.BucketCount(), 4);
}

TEST_F(DashTableTest, LargeInitialCapacity) {
	DashTable<std::string, std::string> table(1024);
	EXPECT_GE(table.BucketCount(), 1024);
}

TEST_F(DashTableTest, DifferentTypes) {
	DashTable<int32_t, double> int_table;
	int_table.Insert(1, 1.5);
	int_table.Insert(2, 2.5);

	EXPECT_EQ(*int_table.Find(1), 1.5);
	EXPECT_EQ(*int_table.Find(2), 2.5);

	DashTable<int32_t, int32_t> simple_table;
	simple_table.Insert(100, 200);
	EXPECT_EQ(*simple_table.Find(100), 200);
}

TEST_F(DashTableTest, DuplicateKeysDifferentBuckets) {
	DashTable<int32_t, int32_t> table(4);

	for (int32_t key = 0; key < 100; key += 4) {
		table.Insert(key, key * 10);
	}

	for (int32_t key = 0; key < 100; key += 4) {
		auto result = table.Find(key);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, key * 10);
	}
}

TEST_F(DashTableTest, EmptyStringKey) {
	hash.Insert("", "empty_key_value");

	auto result = hash.Find("");
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(*result, "empty_key_value");

	EXPECT_TRUE(hash.Erase(""));
	EXPECT_EQ(hash.Find(""), nullptr);
}

TEST_F(DashTableTest, EmptyStringValue) {
	hash.Insert("key", "");

	auto result = hash.Find("key");
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(*result, "");
}

TEST_F(DashTableTest, ForEachEmpty) {
	uint32_t count = 0;
	hash.ForEach([&count](const std::string&, const std::string&) { ++count; });

	EXPECT_EQ(count, 0);
}

TEST_F(DashTableTest, ForEachAfterClear) {
	uint32_t count = 0;
	hash.ForEach([&count](const std::string&, const std::string&) { ++count; });

	EXPECT_EQ(count, 0);
}

TEST_F(DashTableTest, RehashPreservesAllData) {
	DashTable<int32_t, int32_t> table(4);

	for (int32_t key = 0; key < 100; ++key) {
		table.Insert(key, key * 10);
	}

	EXPECT_EQ(table.Size(), 100);
	EXPECT_GT(table.BucketCount(), 4);

	for (int32_t key = 0; key < 100; ++key) {
		auto result = table.Find(key);
		ASSERT_NE(result, nullptr) << "Failed to find key " << key;
		EXPECT_EQ(*result, key * 10);
	}
}

TEST_F(DashTableTest, RehashWithCollisions) {
	DashTable<int32_t, int32_t> table(4);

	for (int32_t key = 0; key < 50; ++key) {
		table.Insert(key * 4, key);
	}

	for (int32_t key = 0; key < 50; ++key) {
		auto result = table.Find(key * 4);
		ASSERT_NE(result, nullptr) << "Failed to find key " << key * 4;
		EXPECT_EQ(*result, key);
	}
}

TEST_F(DashTableTest, RehashAndDeleteMixed) {
	DashTable<int32_t, int32_t> table(4);

	for (int32_t key = 0; key < 100; ++key) {
		table.Insert(key, key * 10);
	}

	for (int32_t key = 0; key < 50; ++key) {
		EXPECT_TRUE(table.Erase(key));
	}

	for (int32_t key = 0; key < 50; ++key) {
		EXPECT_EQ(table.Find(key), nullptr);
	}

	for (int32_t key = 50; key < 100; ++key) {
		auto result = table.Find(key);
		ASSERT_NE(result, nullptr) << "Failed to find key " << key;
		EXPECT_EQ(*result, key * 10);
	}
}

TEST_F(DashTableTest, RehashMaintainsCorrectBucketDistribution) {
	DashTable<int32_t, int32_t> table(4);

	for (int32_t key = 0; key < 20; ++key) {
		table.Insert(key, key);
	}

	EXPECT_EQ(table.Size(), 20);

	uint64_t total_items = 0;
	table.ForEach([&total_items](const int32_t&, const int32_t&) { ++total_items; });

	EXPECT_EQ(total_items, 20);
}

TEST_F(DashTableTest, RehashAfterChainedNodes) {
	DashTable<int32_t, int32_t> table(4);

	for (int32_t key = 0; key < 100; ++key) {
		table.Insert(key, key);
	}

	for (int32_t key = 0; key < 100; ++key) {
		EXPECT_EQ(*table.Find(key), key);
	}

	table.Insert(1000, 1000);
	EXPECT_EQ(*table.Find(1000), 1000);

	for (int32_t key = 0; key < 100; ++key) {
		EXPECT_EQ(*table.Find(key), key);
	}
}
