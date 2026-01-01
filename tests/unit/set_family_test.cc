#include <gtest/gtest.h>
#include "core/compact_obj.h"
#include "core/database.h"
#include "command/set_family.h"

class SetFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db_ = std::make_unique<Database>();
		SetFamily::SetDatabase(db_.get());
	}

	std::unique_ptr<Database> db_;
};

TEST_F(SetFamilyTest, SAddAndSMembers) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	std::string result = SetFamily::SAdd(args);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("myset")};
	result = SetFamily::SMembers(args);
	EXPECT_EQ(result, "*2\r\n$7\r\nmember1\r\n$7\r\nmember2\r\n");
}

TEST_F(SetFamilyTest, SAddDuplicate) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member1")
	};

	std::string result = SetFamily::SAdd(args);
	EXPECT_EQ(result, ":1\r\n");

	args = {CompactObj::fromKey("myset")};
	result = SetFamily::SMembers(args);
	EXPECT_EQ(result, "*1\r\n$7\r\nmember1\r\n");
}

TEST_F(SetFamilyTest, SRem) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1")
	};
	std::string result = SetFamily::SRem(args);
	EXPECT_EQ(result, ":1\r\n");

	args = {CompactObj::fromKey("myset")};
	result = SetFamily::SMembers(args);
	EXPECT_EQ(result, "*1\r\n$7\r\nmember2\r\n");
}

TEST_F(SetFamilyTest, SPop) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args);

	args = {CompactObj::fromKey("myset")};
	std::string result = SetFamily::SPop(args);
	EXPECT_TRUE(result.find("member") != std::string::npos);

	args = {CompactObj::fromKey("myset")};
	result = SetFamily::SCard(args);
	EXPECT_EQ(result, ":1\r\n");
}

TEST_F(SetFamilyTest, SCard) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args);

	args = {CompactObj::fromKey("myset")};
	std::string result = SetFamily::SCard(args);
	EXPECT_EQ(result, ":3\r\n");
}

TEST_F(SetFamilyTest, SIsMember) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1")
	};
	std::string result = SetFamily::SIsMember(args);
	EXPECT_EQ(result, ":1\r\n");

	args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("nonexistent")
	};
	result = SetFamily::SIsMember(args);
	EXPECT_EQ(result, ":0\r\n");
}

TEST_F(SetFamilyTest, SMIsMember) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("nonexistent")
	};
	std::string result = SetFamily::SMIsMember(args);
	EXPECT_EQ(result, "*3\r\n:1\r\n:1\r\n:0\r\n");
}

TEST_F(SetFamilyTest, SInter) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3"),
		CompactObj::fromKey("member4")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2")
	};
	std::string result = SetFamily::SInter(args);
	EXPECT_TRUE(result.find("member2") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
}

TEST_F(SetFamilyTest, SUnion) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2")
	};
	std::string result = SetFamily::SUnion(args);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
	EXPECT_TRUE(result.find("member2") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
}

TEST_F(SetFamilyTest, SDiff) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member4")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2")
	};
	std::string result = SetFamily::SDiff(args);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
	EXPECT_TRUE(result.find("member3") != std::string::npos);
	EXPECT_TRUE(result.find("member2") == std::string::npos);
}

TEST_F(SetFamilyTest, SRandMember) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("myset"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2"),
		CompactObj::fromKey("member3")
	};

	SetFamily::SAdd(args);

	args = {CompactObj::fromKey("myset")};
	std::string result = SetFamily::SRandMember(args);
	EXPECT_TRUE(result.find("member") != std::string::npos);
}

TEST_F(SetFamilyTest, SMove) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("member1"),
		CompactObj::fromKey("member2")
	};

	SetFamily::SAdd(args);

	args = {
		CompactObj::fromKey("set1"),
		CompactObj::fromKey("set2"),
		CompactObj::fromKey("member1")
	};
	std::string result = SetFamily::SMove(args);
	EXPECT_EQ(result, ":1\r\n");

	args = {CompactObj::fromKey("set1")};
	result = SetFamily::SMembers(args);
	EXPECT_TRUE(result.find("member1") == std::string::npos);

	args = {CompactObj::fromKey("set2")};
	result = SetFamily::SMembers(args);
	EXPECT_TRUE(result.find("member1") != std::string::npos);
}

TEST_F(SetFamilyTest, ErrorCases) {
	std::vector<CompactObj> args;
	std::string result = SetFamily::SAdd(args);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
