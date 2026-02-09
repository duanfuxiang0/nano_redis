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

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1")};

	std::string result = HashFamily::HSet(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	args = {NanoObj::FromKey("HGET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1")};
	result = HashFamily::HGet(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");
}

TEST_F(HashFamilyTest, HGetNonExistent) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HGET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("nonexistent")};

	std::string result = HashFamily::HGet(args, &ctx);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(HashFamilyTest, HMSetAndGet) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HMSET"),  NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	std::string result = HashFamily::HMSet(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	args = {NanoObj::FromKey("HMGET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	        NanoObj::FromKey("field2")};
	result = HashFamily::HMGet(args, &ctx);
	EXPECT_EQ(result, "*2\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n");
}

TEST_F(HashFamilyTest, HDel) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HDEL"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1")};
	std::string result = HashFamily::HDel(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	result = HashFamily::HGet({NanoObj::FromKey("HGET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1")}, &ctx);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(HashFamilyTest, HExists) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HEXISTS"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1")};
	std::string result = HashFamily::HExists(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {NanoObj::FromKey("HEXISTS"), NanoObj::FromKey("myhash"), NanoObj::FromKey("nonexistent")};
	result = HashFamily::HExists(args, &ctx);
	EXPECT_EQ(result, ":0\r\n");
}

TEST_F(HashFamilyTest, HLen) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"),   NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HLEN"), NanoObj::FromKey("myhash")};
	std::string result = HashFamily::HLen(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");
}

TEST_F(HashFamilyTest, HKeys) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"),   NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HKEYS"), NanoObj::FromKey("myhash")};
	std::string result = HashFamily::HKeys(args, &ctx);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
}

TEST_F(HashFamilyTest, HVals) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"),   NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HVALS"), NanoObj::FromKey("myhash")};
	std::string result = HashFamily::HVals(args, &ctx);
	EXPECT_TRUE(result.find("value1") != std::string::npos);
	EXPECT_TRUE(result.find("value2") != std::string::npos);
}

TEST_F(HashFamilyTest, HGetAll) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"),   NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HGETALL"), NanoObj::FromKey("myhash")};
	std::string result = HashFamily::HGetAll(args, &ctx);
	EXPECT_TRUE(result.find("field1") != std::string::npos);
	EXPECT_TRUE(result.find("field2") != std::string::npos);
	EXPECT_TRUE(result.find("value1") != std::string::npos);
	EXPECT_TRUE(result.find("value2") != std::string::npos);
}

TEST_F(HashFamilyTest, HIncrBy) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("counter"),
	                             NanoObj::FromKey("10")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HINCRBY"), NanoObj::FromKey("myhash"), NanoObj::FromKey("counter"),
	        NanoObj::FromKey("5")};
	std::string result = HashFamily::HIncrBy(args, &ctx);
	EXPECT_EQ(result, "$2\r\n15\r\n");

	args = {NanoObj::FromKey("HGET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("counter")};
	result = HashFamily::HGet(args, &ctx);
	EXPECT_EQ(result, "$2\r\n15\r\n");
}

TEST_F(HashFamilyTest, HStrLen) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HSTRLEN"), NanoObj::FromKey("myhash"), NanoObj::FromKey("field1")};
	std::string result = HashFamily::HStrLen(args, &ctx);
	EXPECT_EQ(result, ":6\r\n");
}

TEST_F(HashFamilyTest, HRandField) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"),   NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HRANDFIELD"), NanoObj::FromKey("myhash")};
	std::string result = HashFamily::HRandField(args, &ctx);
	EXPECT_TRUE(result.find("field") != std::string::npos || result.find("value") != std::string::npos);
}

TEST_F(HashFamilyTest, HScan) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("HSET"),   NanoObj::FromKey("myhash"), NanoObj::FromKey("field1"),
	                             NanoObj::FromKey("value1"), NanoObj::FromKey("field2"), NanoObj::FromKey("value2")};

	HashFamily::HSet(args, &ctx);

	args = {NanoObj::FromKey("HSCAN"), NanoObj::FromKey("myhash"), NanoObj::FromKey("0")};
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
