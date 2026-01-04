#include <gtest/gtest.h>
#include "core/compact_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "command/set_family.h"

class SetFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db_ = std::make_unique<Database>();
	}

	std::unique_ptr<Database> db_;
};

TEST_F(SetFamilyTest, SAddAndSMembers) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	std::string result = SetFamily::SAdd(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("SMEMBERS"), CompactObj::fromKey("myset")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_EQ(result, "*2\r\n$7\r\nmember1\r\n$7\r\nmember2\r\n");
}

TEST_F(SetFamilyTest, SAddDuplicate) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member1")
	};

	std::string result = SetFamily::SAdd(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {CompactObj::fromKey("SMEMBERS"), CompactObj::fromKey("myset")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_EQ(result, "*1\r\n$7\r\nmember1\r\n");
}

TEST_F(SetFamilyTest, SRem) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SREM"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1")
	};
	std::string result = SetFamily::SRem(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {CompactObj::fromKey("SMEMBERS"), CompactObj::fromKey("myset")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_EQ(result, "*1\r\n$7\r\nmember2\r\n");
}

TEST_F(SetFamilyTest, SPop) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args, &ctx);

	args = {CompactObj::fromKey("SPOP"), CompactObj::fromKey("myset")};
	std::string result = SetFamily::SPop(args, &ctx);
	EXPECT_TRUE(result.find("member") != std::string::npos);

	args = {CompactObj::fromKey("SCARD"), CompactObj::fromKey("myset")};
	result = SetFamily::SCard(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");
}

TEST_F(SetFamilyTest, SCard) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args, &ctx);

	args = {CompactObj::fromKey("SCARD"), CompactObj::fromKey("myset")};
	std::string result = SetFamily::SCard(args, &ctx);
	EXPECT_EQ(result, ":3\r\n");
}

TEST_F(SetFamilyTest, SIsMember) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SISMEMBER"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1")
	};
	std::string result = SetFamily::SIsMember(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {
		CompactObj::fromKey("SISMEMBER"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("nonexistent")
	};
	result = SetFamily::SIsMember(args, &ctx);
	EXPECT_EQ(result, ":0\r\n");
}

TEST_F(SetFamilyTest, SMIsMember) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SMISMEMBER"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("nonexistent")
	};
	std::string result = SetFamily::SMIsMember(args, &ctx);
	EXPECT_EQ(result, "*3\r\n:1\r\n:1\r\n:0\r\n");
}

TEST_F(SetFamilyTest, SInter) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3"),
		CompactObj::fromKey("member4")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SINTER"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2")
	};
	std::string result = SetFamily::SInter(args, &ctx);
	EXPECT_TRUE(result.find("member2") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
}

TEST_F(SetFamilyTest, SUnion) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SUNION"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2")
	};
	std::string result = SetFamily::SUnion(args, &ctx);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
	EXPECT_TRUE(result.find("member2") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
}

TEST_F(SetFamilyTest, SDiff) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member4")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SDIFF"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2")
	};
	std::string result = SetFamily::SDiff(args, &ctx);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
	EXPECT_TRUE(result.find("member2") == std::string::npos);
}

TEST_F(SetFamilyTest, SRandMember) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args, &ctx);

	args = {CompactObj::fromKey("SRANDMEMBER"), CompactObj::fromKey("myset")};
	std::string result = SetFamily::SRandMember(args, &ctx);
	EXPECT_TRUE(result.find("member") != std::string::npos);
}

TEST_F(SetFamilyTest, SMove) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("SADD"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args, &ctx);

	args = {
		CompactObj::fromKey("SMOVE"),
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member1")
	};
	std::string result = SetFamily::SMove(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");

	args = {CompactObj::fromKey("SMEMBERS"), CompactObj::fromKey("set1")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_TRUE(result.find("member1") == std::string::npos);

	args = {CompactObj::fromKey("SMEMBERS"), CompactObj::fromKey("set2")};
	result = SetFamily::SMembers(args, &ctx);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
}

TEST_F(SetFamilyTest, ErrorCases) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args;
	std::string result = SetFamily::SAdd(args, &ctx);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
