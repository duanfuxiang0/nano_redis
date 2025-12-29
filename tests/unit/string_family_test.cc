#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "command/string_family.h"
#include "command/command_registry.h"

class StringFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		registry_ = &CommandRegistry::instance();
		StringFamily::Register(registry_);
		StringFamily::ClearDatabase();
	}

	std::string Execute(const std::string& cmd, const std::vector<std::string>& args) {
		std::vector<std::string> full_args = {cmd};
		full_args.insert(full_args.end(), args.begin(), args.end());
		return registry_->execute(full_args);
	}

	CommandRegistry* registry_;
};

TEST_F(StringFamilyTest, SetAndGet) {
	EXPECT_EQ(Execute("SET", {"key1", "value1"}), "+OK\r\n");
	EXPECT_EQ(Execute("GET", {"key1"}), "$6\r\nvalue1\r\n");
}

TEST_F(StringFamilyTest, GetNonExistent) {
	EXPECT_EQ(Execute("GET", {"nonexistent"}), "$-1\r\n");
}

TEST_F(StringFamilyTest, SetOverwrite) {
	Execute("SET", {"key1", "value1"});
	Execute("SET", {"key1", "value2"});
	EXPECT_EQ(Execute("GET", {"key1"}), "$6\r\nvalue2\r\n");
}

TEST_F(StringFamilyTest, Delete) {
	Execute("SET", {"key1", "value1"});
	Execute("SET", {"key2", "value2"});
	EXPECT_EQ(Execute("DEL", {"key1"}), ":1\r\n");
	EXPECT_EQ(Execute("GET", {"key1"}), "$-1\r\n");
	EXPECT_EQ(Execute("GET", {"key2"}), "$6\r\nvalue2\r\n");
}

TEST_F(StringFamilyTest, DeleteMultiple) {
	Execute("SET", {"key1", "value1"});
	Execute("SET", {"key2", "value2"});
	Execute("SET", {"key3", "value3"});
	EXPECT_EQ(Execute("DEL", {"key1", "key2"}), ":2\r\n");
	EXPECT_EQ(Execute("DEL", {"key4"}), ":0\r\n");
}

TEST_F(StringFamilyTest, Exists) {
	Execute("SET", {"key1", "value1"});
	Execute("SET", {"key2", "value2"});
	EXPECT_EQ(Execute("EXISTS", {"key1"}), ":1\r\n");
	EXPECT_EQ(Execute("EXISTS", {"key1", "key2"}), ":2\r\n");
	EXPECT_EQ(Execute("EXISTS", {"key3"}), ":0\r\n");
}

TEST_F(StringFamilyTest, MSetAndGet) {
	EXPECT_EQ(Execute("MSET", {"key1", "value1", "key2", "value2"}), "+OK\r\n");
	EXPECT_EQ(Execute("GET", {"key1"}), "$6\r\nvalue1\r\n");
	EXPECT_EQ(Execute("GET", {"key2"}), "$6\r\nvalue2\r\n");
}

TEST_F(StringFamilyTest, MGet) {
	Execute("SET", {"key1", "value1"});
	Execute("SET", {"key2", "value2"});

	std::string response = Execute("MGET", {"key1", "key2", "key3"});
	EXPECT_TRUE(response.find("*3") == 0);
	EXPECT_TRUE(response.find("value1") != std::string::npos);
	EXPECT_TRUE(response.find("value2") != std::string::npos);
}

TEST_F(StringFamilyTest, Incr) {
	Execute("SET", {"counter", "10"});
	EXPECT_EQ(Execute("INCR", {"counter"}), ":11\r\n");
	EXPECT_EQ(Execute("GET", {"counter"}), "$2\r\n11\r\n");
}

TEST_F(StringFamilyTest, IncrNewKey) {
	EXPECT_EQ(Execute("INCR", {"newkey"}), ":1\r\n");
	EXPECT_EQ(Execute("GET", {"newkey"}), "$1\r\n1\r\n");
}

TEST_F(StringFamilyTest, Decr) {
	Execute("SET", {"counter", "10"});
	EXPECT_EQ(Execute("DECR", {"counter"}), ":9\r\n");
	EXPECT_EQ(Execute("GET", {"counter"}), "$1\r\n9\r\n");
}

TEST_F(StringFamilyTest, IncrBy) {
	Execute("SET", {"counter", "10"});
	EXPECT_EQ(Execute("INCRBY", {"counter", "5"}), ":15\r\n");
	EXPECT_EQ(Execute("INCRBY", {"counter", "-3"}), ":12\r\n");
}

TEST_F(StringFamilyTest, DecrBy) {
	Execute("SET", {"counter", "10"});
	EXPECT_EQ(Execute("DECRBY", {"counter", "5"}), ":5\r\n");
}

TEST_F(StringFamilyTest, Append) {
	Execute("SET", {"key", "Hello"});
	EXPECT_EQ(Execute("APPEND", {"key", " World"}), ":11\r\n");
	EXPECT_EQ(Execute("GET", {"key"}), "$11\r\nHello World\r\n");
}

TEST_F(StringFamilyTest, AppendNewKey) {
	EXPECT_EQ(Execute("APPEND", {"newkey", "Hello"}), ":5\r\n");
	EXPECT_EQ(Execute("GET", {"newkey"}), "$5\r\nHello\r\n");
}

TEST_F(StringFamilyTest, StrLen) {
	Execute("SET", {"key", "Hello World"});
	EXPECT_EQ(Execute("STRLEN", {"key"}), ":11\r\n");
	EXPECT_EQ(Execute("STRLEN", {"nonexistent"}), ":0\r\n");
}

TEST_F(StringFamilyTest, GetRange) {
	Execute("SET", {"key", "Hello World"});
	EXPECT_EQ(Execute("GETRANGE", {"key", "0", "4"}), "$5\r\nHello\r\n");
	EXPECT_EQ(Execute("GETRANGE", {"key", "6", "10"}), "$5\r\nWorld\r\n");
	EXPECT_EQ(Execute("GETRANGE", {"key", "-6", "-1"}), "$6\r\n World\r\n");
}

TEST_F(StringFamilyTest, GetRangeOutOfBounds) {
	Execute("SET", {"key", "Hello"});
	EXPECT_EQ(Execute("GETRANGE", {"key", "0", "100"}), "$5\r\nHello\r\n");
	EXPECT_EQ(Execute("GETRANGE", {"key", "-100", "100"}), "$5\r\nHello\r\n");
}

TEST_F(StringFamilyTest, SetRange) {
	Execute("SET", {"key", "Hello World"});
	EXPECT_EQ(Execute("SETRANGE", {"key", "6", "Redis"}), ":11\r\n");
	EXPECT_EQ(Execute("GET", {"key"}), "$11\r\nHello Redis\r\n");
}

TEST_F(StringFamilyTest, SelectAndDBSize) {
	Execute("SET", {"db0key", "value0"});
	EXPECT_EQ(Execute("SELECT", {"1"}), "+OK\r\n");
	Execute("SET", {"db1key", "value1"});

	EXPECT_EQ(Execute("GET", {"db0key"}), "$-1\r\n");
	EXPECT_EQ(Execute("GET", {"db1key"}), "$6\r\nvalue1\r\n");

	EXPECT_EQ(Execute("SELECT", {"0"}), "+OK\r\n");
	EXPECT_EQ(Execute("GET", {"db0key"}), "$6\r\nvalue0\r\n");
}

TEST_F(StringFamilyTest, ErrorCases) {
	EXPECT_TRUE(Execute("SET", {}).find("wrong number") != std::string::npos);
	EXPECT_TRUE(Execute("GET", {}).find("wrong number") != std::string::npos);
	EXPECT_TRUE(Execute("INCR", {}).find("wrong number") != std::string::npos);
	EXPECT_TRUE(Execute("APPEND", {}).find("wrong number") != std::string::npos);
}
