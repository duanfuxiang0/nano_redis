#include <gtest/gtest.h>
#include "core/compact_obj.h"
#include "core/database.h"
#include "command/hash_family.h"

class HashFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db_ = std::make_unique<Database>();
		HashFamily::SetDatabase(db_.get());
	}

	std::unique_ptr<Database> db_;
};

TEST_F(HashFamilyTest, HSetAndGet) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1")
	};

	std::string result = HashFamily::HSet(args);
	EXPECT_EQ(result, "+OK\r\n");

	args = {CompactObj::fromKey("myhash"), CompactObj::fromKey("field1")};
	result = HashFamily::HGet(args);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");
}

TEST_F(HashFamilyTest, HGetNonExistent) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("nonexistent")
	};

	std::string result = HashFamily::HGet(args);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(HashFamilyTest, HMSetAndGet) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	std::string result = HashFamily::HMSet(args);
	EXPECT_EQ(result, "+OK\r\n");

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("field2")
	};
	result = HashFamily::HMGet(args);
	EXPECT_EQ(result, "*2\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n");
}

TEST_F(HashFamilyTest, HDel) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1")
	};

	HashFamily::HSet(args);

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1")
	};
	std::string result = HashFamily::HDel(args);
	EXPECT_EQ(result, ":1\r\n");

	result = HashFamily::HGet(args);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(HashFamilyTest, HExists) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1")
	};

	HashFamily::HSet(args);

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1")
	};
	std::string result = HashFamily::HExists(args);
	EXPECT_EQ(result, ":1\r\n");

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("nonexistent")
	};
	result = HashFamily::HExists(args);
	EXPECT_EQ(result, ":0\r\n");
}

TEST_F(HashFamilyTest, HLen) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	HashFamily::HSet(args);

	args = {CompactObj::fromKey("myhash")};
	std::string result = HashFamily::HLen(args);
	EXPECT_EQ(result, ":2\r\n");
}

TEST_F(HashFamilyTest, HKeys) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	HashFamily::HSet(args);

	args = {CompactObj::fromKey("myhash")};
	std::string result = HashFamily::HKeys(args);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
}

TEST_F(HashFamilyTest, HVals) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	HashFamily::HSet(args);

	args = {CompactObj::fromKey("myhash")};
	std::string result = HashFamily::HVals(args);
	EXPECT_TRUE(result.find("value1") != std::string::npos);
	EXPECT_TRUE(result.find("value2") != std::string::npos);
}

TEST_F(HashFamilyTest, HGetAll) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	HashFamily::HSet(args);

	args = {CompactObj::fromKey("myhash")};
	std::string result = HashFamily::HGetAll(args);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
	EXPECT_TRUE(result.find("value1") != std::string::npos);
	EXPECT_TRUE(result.find("value2") != std::string::npos);
}

TEST_F(HashFamilyTest, HIncrBy) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("counter"),
		CompactObj::fromKey("10")
	};

	HashFamily::HSet(args);

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("counter"),
		CompactObj::fromKey("5")
	};
	std::string result = HashFamily::HIncrBy(args);
	EXPECT_EQ(result, "$2\r\n15\r\n");

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("counter")
	};
	result = HashFamily::HGet(args);
	EXPECT_EQ(result, "$2\r\n15\r\n");
}

TEST_F(HashFamilyTest, HStrLen) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1")
	};

	HashFamily::HSet(args);

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1")
	};
	std::string result = HashFamily::HStrLen(args);
	EXPECT_EQ(result, ":6\r\n");
}

TEST_F(HashFamilyTest, HRandField) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	HashFamily::HSet(args);

	args = {CompactObj::fromKey("myhash")};
	std::string result = HashFamily::HRandField(args);
	EXPECT_TRUE(result.find("field") != std::string::npos || result.find("value") != std::string::npos);
}

TEST_F(HashFamilyTest, HScan) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("field1"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("field2"),
		CompactObj::fromKey("value2")
	};

	HashFamily::HSet(args);

	args = {
		CompactObj::fromKey("myhash"),
		CompactObj::fromKey("0")
	};
	std::string result = HashFamily::HScan(args);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
}

TEST_F(HashFamilyTest, ErrorCases) {
	std::vector<CompactObj> args;
	std::string result = HashFamily::HSet(args);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
