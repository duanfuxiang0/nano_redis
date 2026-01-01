#include <gtest/gtest.h>
#include "core/compact_obj.h"
#include "core/database.h"
#include "command/list_family.h"

class ListFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		db_ = std::make_unique<Database>();
		ListFamily::SetDatabase(db_.get());
	}

	std::unique_ptr<Database> db_;
};

TEST_F(ListFamilyTest, LPushAndRPop) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2")
	};

	std::string result = ListFamily::LPush(args);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("mylist")};
	result = ListFamily::RPop(args);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	result = ListFamily::RPop(args);
	EXPECT_EQ(result, "$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, RPushAndLPop) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2")
	};

	std::string result = ListFamily::RPush(args);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("mylist")};
	result = ListFamily::LPop(args);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	result = ListFamily::LPop(args);
	EXPECT_EQ(result, "$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, LLen) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::LPush(args);

	args = {CompactObj::fromKey("mylist")};
	std::string result = ListFamily::LLen(args);
	EXPECT_EQ(result, ":3\r\n");
}

TEST_F(ListFamilyTest, LIndex) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0")
	};
	std::string result = ListFamily::LIndex(args);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LIndex(args);
	EXPECT_EQ(result, "$6\r\nvalue3\r\n");
}

TEST_F(ListFamilyTest, LSet) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("newvalue1")
	};
	std::string result = ListFamily::LSet(args);
	EXPECT_EQ(result, "+OK\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0")
	};
	std::string result2 = ListFamily::LIndex(args);
	EXPECT_EQ(result2, "$9\r\nnewvalue1\r\n");
}

TEST_F(ListFamilyTest, LRange) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("1")
	};
	std::string result = ListFamily::LRange(args);
	EXPECT_EQ(result, "*2\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LRange(args);
	EXPECT_EQ(result, "*3\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n$6\r\nvalue3\r\n");
}

TEST_F(ListFamilyTest, LTrim) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("1"),
		CompactObj::fromKey("1")
	};
	std::string result = ListFamily::LTrim(args);
	EXPECT_EQ(result, "+OK\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LRange(args);
	EXPECT_EQ(result, "*1\r\n$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, LRem) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value1")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("value1")
	};
	std::string result = ListFamily::LRem(args);
	EXPECT_EQ(result, ":2\r\n");

	args = {CompactObj::fromKey("mylist")};
	result = ListFamily::LLen(args);
	EXPECT_EQ(result, ":1\r\n");
}

TEST_F(ListFamilyTest, LInsert) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("BEFORE"),
		CompactObj::fromKey("value3"),
		CompactObj::fromKey("value2")
	};
	std::string result = ListFamily::LInsert(args);
	EXPECT_EQ(result, ":3\r\n");

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("0"),
		CompactObj::fromKey("-1")
	};
	result = ListFamily::LRange(args);
	EXPECT_EQ(result, "*3\r\n$6\r\nvalue1\r\n$6\r\nvalue2\r\n$6\r\nvalue3\r\n");
}

TEST_F(ListFamilyTest, LPopEmpty) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1")
	};

	ListFamily::RPush(args);

	args = {CompactObj::fromKey("mylist")};
	std::string result = ListFamily::LPop(args);
	EXPECT_EQ(result, "$6\r\nvalue1\r\n");

	result = ListFamily::LPop(args);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(ListFamilyTest, LIndexOutOfRange) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("10")
	};
	std::string result = ListFamily::LIndex(args);
	EXPECT_EQ(result, "$-1\r\n");
}

TEST_F(ListFamilyTest, LIndexNegative) {
	std::vector<CompactObj> args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("value1"),
		CompactObj::fromKey("value2"),
		CompactObj::fromKey("value3")
	};

	ListFamily::RPush(args);

	args = {
		CompactObj::fromKey("mylist"),
		CompactObj::fromKey("-2")
	};
	std::string result = ListFamily::LIndex(args);
	EXPECT_EQ(result, "$6\r\nvalue2\r\n");
}

TEST_F(ListFamilyTest, ErrorCases) {
	std::vector<CompactObj> args;
	std::string result = ListFamily::LPush(args);
	EXPECT_TRUE(result.find("wrong number of arguments") != std::string::npos);
}
