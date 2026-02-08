#include <gtest/gtest.h>
#include "core/nano_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "command/hash_family.h"

class HashFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db = std::make_unique<Database>();
	}

	std::unique_ptr<Database> db;
};

TEST_F(HashFamilyTest, HSetAndGet) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1")};

	std::string result = HashFamily::HSet(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	args = {NanoObj::fromKey("HGET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1")};
	result = HashFamily::HGet(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");
}

TEST_F(HashFamilyTest, HGetNonExistent) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HGET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("nonexistent")};

	std::string result = HashFamily::HGet(args, &ctx);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(HashFamilyTest, HMSetAndGet) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HMSET"),  NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	std::string result = HashFamily::HMSet(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	args = {NanoObj::fromKey("HMGET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	        NanoObj::fromKey("field2")};
	result = HashFamily::HMGet(args, &ctx);
	EXPECT_EQ(result, "*2\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n");
}

TEST_F(HashFamilyTest, HDel) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HDEL"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1")};
	std::string result = HashFamily::HDel(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	result = HashFamily::HGet({NanoObj::fromKey("HGET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1")}, &ctx);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(HashFamilyTest, HExists) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HEXISTS"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1")};
	std::string result = HashFamily::HExists(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {NanoObj::fromKey("HEXISTS"), NanoObj::fromKey("myhash"), NanoObj::fromKey("nonexistent")};
	result = HashFamily::HExists(args, &ctx);
	EXPECT_EQ(result, ":0\r\n");
}

TEST_F(HashFamilyTest, HLen) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"),   NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HLEN"), NanoObj::fromKey("myhash")};
	std::string result = HashFamily::HLen(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");
}

TEST_F(HashFamilyTest, HKeys) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"),   NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HKEYS"), NanoObj::fromKey("myhash")};
	std::string result = HashFamily::HKeys(args, &ctx);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
}

TEST_F(HashFamilyTest, HVals) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"),   NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HVALS"), NanoObj::fromKey("myhash")};
	std::string result = HashFamily::HVals(args, &ctx);
	EXPECT_TRUE(result.find("value1") != std::string::npos);
	EXPECT_TRUE(result.find("value2") != std::string::npos);
}

TEST_F(HashFamilyTest, HGetAll) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"),   NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HGETALL"), NanoObj::fromKey("myhash")};
	std::string result = HashFamily::HGetAll(args, &ctx);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
	EXPECT_TRUE(result.find("value1") != std::string::npos);
	EXPECT_TRUE(result.find("value2") != std::string::npos);
}

TEST_F(HashFamilyTest, HIncrBy) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("counter"),
	                             NanoObj::fromKey("10")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HINCRBY"), NanoObj::fromKey("myhash"), NanoObj::fromKey("counter"),
	        NanoObj::fromKey("5")};
	std::string result = HashFamily::HIncrBy(args, &ctx);
	EXPECT_EQ(result, "$2\r\n15\r\n");

	args = {NanoObj::fromKey("HGET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("counter")};
	result = HashFamily::HGet(args, &ctx);
	EXPECT_EQ(result, "$2\r\n15\r\n");
}

TEST_F(HashFamilyTest, HStrLen) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HSTRLEN"), NanoObj::fromKey("myhash"), NanoObj::fromKey("field1")};
	std::string result = HashFamily::HStrLen(args, &ctx);
	EXPECT_EQ(result, ":6\r\n");
}

TEST_F(HashFamilyTest, HRandField) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"),   NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HRANDFIELD"), NanoObj::fromKey("myhash")};
	std::string result = HashFamily::HRandField(args, &ctx);
	EXPECT_TRUE(result.find("field") != std::string::npos || result.find("value") != std::string::npos);
}

TEST_F(HashFamilyTest, HScan) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::fromKey("HSET"),   NanoObj::fromKey("myhash"), NanoObj::fromKey("field1"),
	                             NanoObj::fromKey("value1"), NanoObj::fromKey("field2"), NanoObj::fromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::fromKey("HSCAN"), NanoObj::fromKey("myhash"), NanoObj::fromKey("0")};
	std::string result = HashFamily::HScan(args, &ctx);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
}

TEST_F(HashFamilyTest, ErrorCases) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args;
	std::string result = HashFamily::HSet(args, &ctx);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
