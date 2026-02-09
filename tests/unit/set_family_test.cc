#include <gtest/gtest.h>
#include "core/nano_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "command/set_family.h"

class SetFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db = std::make_unique<Database>();
	}

	std::unique_ptr<Database> db;
};

TEST_F(SetFamilyTest, SAddAndSMembers) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2")};

	std::string result = SetFamily::SAdd(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");

	args = {NanoObj::FromKey("SMEMBERS"), NanoObj::FromKey("myset")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_EQ(result, "*2\r\n$7\r\nmember1\r\n$7\r\nmember2\r\n");
}

TEST_F(SetFamilyTest, SAddDuplicate) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member1")};

	std::string result = SetFamily::SAdd(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {NanoObj::FromKey("SMEMBERS"), NanoObj::FromKey("myset")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_EQ(result, "*1\r\n$7\r\nmember1\r\n");
}

TEST_F(SetFamilyTest, SRem) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SREM"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1")};
	std::string result = SetFamily::SRem(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {NanoObj::FromKey("SMEMBERS"), NanoObj::FromKey("myset")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_EQ(result, "*1\r\n$7\r\nmember2\r\n");
}

TEST_F(SetFamilyTest, SPop) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SPOP"), NanoObj::FromKey("myset")};
	std::string result = SetFamily::SPop(args, &ctx);
	EXPECT_TRUE(result.find("member") != std::string::npos);

	args = {NanoObj::FromKey("SCARD"), NanoObj::FromKey("myset")};
	result = SetFamily::SCard(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");
}

TEST_F(SetFamilyTest, SCard) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2"), NanoObj::FromKey("member3")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SCARD"), NanoObj::FromKey("myset")};
	std::string result = SetFamily::SCard(args, &ctx);
	EXPECT_EQ(result, ":3\r\n");
}

TEST_F(SetFamilyTest, SIsMember) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SISMEMBER"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1")};
	std::string result = SetFamily::SIsMember(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {NanoObj::FromKey("SISMEMBER"), NanoObj::FromKey("myset"), NanoObj::FromKey("nonexistent")};
	result = SetFamily::SIsMember(args, &ctx);
	EXPECT_EQ(result, ":0\r\n");
}

TEST_F(SetFamilyTest, SMIsMember) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2"), NanoObj::FromKey("member3")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SMISMEMBER"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	        NanoObj::FromKey("member2"), NanoObj::FromKey("nonexistent")};
	std::string result = SetFamily::SMIsMember(args, &ctx);
	EXPECT_EQ(result, "*3\r\n:1\r\n:1\r\n:0\r\n");
}

TEST_F(SetFamilyTest, SInter) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set1"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2"), NanoObj::FromKey("member3")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set2"), NanoObj::FromKey("member2"),
	        NanoObj::FromKey("member3"), NanoObj::FromKey("member4")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SINTER"), NanoObj::FromKey("set1"), NanoObj::FromKey("set2")};
	std::string result = SetFamily::SInter(args, &ctx);
	EXPECT_TRUE(result.find("member2") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
}

TEST_F(SetFamilyTest, SUnion) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set1"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set2"), NanoObj::FromKey("member2"),
	        NanoObj::FromKey("member3")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SUNION"), NanoObj::FromKey("set1"), NanoObj::FromKey("set2")};
	std::string result = SetFamily::SUnion(args, &ctx);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
	EXPECT_TRUE(result.find("member2") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
}

TEST_F(SetFamilyTest, SDiff) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set1"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2"), NanoObj::FromKey("member3")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set2"), NanoObj::FromKey("member2"),
	        NanoObj::FromKey("member4")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SDIFF"), NanoObj::FromKey("set1"), NanoObj::FromKey("set2")};
	std::string result = SetFamily::SDiff(args, &ctx);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
	EXPECT_TRUE(result.find("member2") == std::string::npos);
}

TEST_F(SetFamilyTest, SRandMember) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("myset"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2"), NanoObj::FromKey("member3")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SRANDMEMBER"), NanoObj::FromKey("myset")};
	std::string result = SetFamily::SRandMember(args, &ctx);
	EXPECT_TRUE(result.find("member") != std::string::npos);
}

TEST_F(SetFamilyTest, SMove) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args = {NanoObj::FromKey("SADD"), NanoObj::FromKey("set1"), NanoObj::FromKey("member1"),
	                             NanoObj::FromKey("member2")};

	SetFamily::SAdd(args, &ctx);

	args = {NanoObj::FromKey("SMOVE"), NanoObj::FromKey("set1"), NanoObj::FromKey("set2"), NanoObj::FromKey("member1")};
	std::string result = SetFamily::SMove(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {NanoObj::FromKey("SMEMBERS"), NanoObj::FromKey("set1")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_TRUE(result.find("member1") == std::string::npos);

	args = {NanoObj::FromKey("SMEMBERS"), NanoObj::FromKey("set2")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
}

TEST_F(SetFamilyTest, ErrorCases) {
	CommandContext ctx(db.get(), 0);

	std::vector<NanoObj> args;
	std::string result = SetFamily::SAdd(args, &ctx);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
