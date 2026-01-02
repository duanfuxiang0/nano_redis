#include <gtest/gtest.h>
#include "server/sharding.h"
#include <unordered_map>
#include <algorithm>

TEST(ShardingTest, SingleShard) {
	EXPECT_EQ(Shard("key1", 1), 0);
	EXPECT_EQ(Shard("key2", 1), 0);
	EXPECT_EQ(Shard("any_key", 1), 0);
}

TEST(ShardingTest, TwoShards) {
	size_t shard0_count = 0;
	size_t shard1_count = 0;

	for (int i = 0; i < 1000; ++i) {
		std::string key = "key_" + std::to_string(i);
		size_t shard_id = Shard(key, 2);
		EXPECT_TRUE(shard_id == 0 || shard_id == 1);

		if (shard_id == 0) {
			shard0_count++;
		} else {
			shard1_count++;
		}
	}

	EXPECT_GT(shard0_count, 300);
	EXPECT_GT(shard1_count, 300);
	EXPECT_LT(shard0_count, 700);
	EXPECT_LT(shard1_count, 700);
}

TEST(ShardingTest, FourShards) {
	std::unordered_map<size_t, size_t> shard_counts;

	for (int i = 0; i < 1000; ++i) {
		std::string key = "key_" + std::to_string(i);
		size_t shard_id = Shard(key, 4);
		EXPECT_TRUE(shard_id >= 0 && shard_id < 4);

		shard_counts[shard_id]++;
	}

	EXPECT_EQ(shard_counts.size(), 4);

	for (size_t i = 0; i < 4; ++i) {
		EXPECT_GT(shard_counts[i], 150) << "Shard " << i << " has " << shard_counts[i] << " keys";
		EXPECT_LT(shard_counts[i], 450) << "Shard " << i << " has " << shard_counts[i] << " keys";
	}
}

TEST(ShardingTest, ConsistentHashing) {
	std::string key = "test_key";

	size_t shard_id_4 = Shard(key, 4);
	size_t shard_id_8 = Shard(key, 8);
	size_t shard_id_16 = Shard(key, 16);

	EXPECT_TRUE(shard_id_4 < 4);
	EXPECT_TRUE(shard_id_8 < 8);
	EXPECT_TRUE(shard_id_16 < 16);

	EXPECT_EQ(Shard(key, 4), shard_id_4);
	EXPECT_EQ(Shard(key, 8), shard_id_8);
	EXPECT_EQ(Shard(key, 16), shard_id_16);
}

TEST(ShardingTest, EmptyKey) {
	EXPECT_EQ(Shard("", 4), Shard("", 4));
	EXPECT_TRUE(Shard("", 4) < 4);
}

TEST(ShardingTest, LongKey) {
	std::string long_key(10000, 'a');
	size_t shard_id = Shard(long_key, 8);
	EXPECT_TRUE(shard_id < 8);

	EXPECT_EQ(Shard(long_key, 8), shard_id);
}
