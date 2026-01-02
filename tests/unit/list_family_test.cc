#include <gtest/gtest.h>
#include "core/compact_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "command/list_family.h"

class ListFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db_ = std::make_unique<Database>();
	}

	std::unique_ptr<Database> db_;
};

TEST_F(ListFamilyTest, LPushAndRPop) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2")
	};

	std::string result = ListFamily::LPush(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("mylist")};
	result = ListFamily::RPop(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	result = ListFamily::RPop(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, RPushAndLPop) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2")
	};

	std::string result = ListFamily::RPush(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("mylist")};
	result = ListFamily::LPop(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	result = ListFamily::LPop(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, LLen) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::LPush(args, &ctx);

	args = {CompactObj::fromKey("mylist")};
	std::string result = ListFamily::LLen(args, &ctx);
	EXPECT_EQ(result, ":3\r\n");
}

TEST_F(ListFamilyTest, LIndex) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0")
	};
	std::string result = ListFamily::LIndex(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LIndex(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue3\r\n");
}

TEST_F(ListFamilyTest, LSet) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("newvalue1")
	};
	std::string result = ListFamily::LSet(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0")
	};
	std::string result2 = ListFamily::LIndex(args, &ctx);
	EXPECT_EQ(result2, "$9\r\nnewvalue1\r\n");
}

TEST_F(ListFamilyTest, LRange) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("1")
	};
	std::string result = ListFamily::LRange(args, &ctx);
	EXPECT_EQ(result, "*2\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LRange(args, &ctx);
	EXPECT_EQ(result, "*3\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n$6\r\nvalue3\r\n");
}

TEST_F(ListFamilyTest, LTrim) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("1"),
		CompactObj::fromKey("1")
	};
	std::string result = ListFamily::LTrim(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LRange(args, &ctx);
	EXPECT_EQ(result, "*1\r\n$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, LRem) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value1")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("value1")
	};
	std::string result = ListFamily::LRem(args, &ctx);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("mylist")};
	result = ListFamily::LLen(args, &ctx);
	EXPECT_EQ(result, ":1\r\n");
}

TEST_F(ListFamilyTest, LInsert) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("BEFORE"),
		CompactObj::fromKey("value3"),
		CompactObj::fromKey("value2")
	};
	std::string result = ListFamily::LInsert(args, &ctx);
	EXPECT_EQ(result, ":3\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LRange(args, &ctx);
	EXPECT_EQ(result, "*3\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n$6\r\nvalue3\r\n");
}

TEST_F(ListFamilyTest, LPopEmpty) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1")
	};

	ListFamily::RPush(args, &ctx);

	args = {CompactObj::fromKey("mylist")};
	std::string result = ListFamily::LPop(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	result = ListFamily::LPop(args, &ctx);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(ListFamilyTest, LIndexOutOfRange) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("10")
	};
	std::string result = ListFamily::LIndex(args, &ctx);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(ListFamilyTest, LIndexNegative) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args, &ctx);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("-2")
	};
	std::string result = ListFamily::LIndex(args, &ctx);
	EXPECT_EQ(result, "$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, ErrorCases) {
	CommandContext ctx(db_.get(), 0);

	std::vector<CompactObj> args;
	std::string result = ListFamily::LPush(args, &ctx);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
