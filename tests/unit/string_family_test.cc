#include <gtest/gtest.h>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include "command/hash_family.h"
#include "command/string_family.h"
#include "command/command_registry.h"
#include "core/nano_obj.h"
#include "core/database.h"
#include "core/command_context.h"
#include "server/connection.h"

class StringFamilyTest : public ::testing::Test {
protected:
	void SetUp() override {
		registry = &CommandRegistry::Instance();
		StringFamily::Register(registry);
		CommandContext ctx(&db, 0);
		StringFamily::ClearDatabase(&ctx);
	}

	std::string Execute(const std::string& cmd, const std::vector<std::string>& args) {
		std::vector<NanoObj> full_args;
		full_args.reserve(1 + args.size());
		full_args.emplace_back(NanoObj::FromKey(cmd));
		for (const auto& arg : args) {
			full_args.emplace_back(NanoObj::FromKey(arg));
		}
		CommandContext ctx(&db, db.CurrentDB());
		return registry->Execute(full_args, &ctx);
	}

	std::string ExecuteWithConnection(const std::string& cmd, const std::vector<std::string>& args, Connection* conn) {
		std::vector<NanoObj> full_args;
		full_args.reserve(1 + args.size());
		full_args.emplace_back(NanoObj::FromKey(cmd));
		for (const auto& arg : args) {
			full_args.emplace_back(NanoObj::FromKey(arg));
		}
		CommandContext ctx(&db, conn->GetDBIndex(), conn);
		return registry->Execute(full_args, &ctx);
	}

	std::optional<int64_t> ParseIntegerResponse(const std::string& response) {
		if (response.size() < 4 || response[0] != ':' || response[response.size() - 2] != '\r' ||
		    response[response.size() - 1] != '\n') {
			return std::nullopt;
		}
		int64_t value = 0;
		const char* start = response.data() + 1;
		const char* end = response.data() + response.size() - 2;
		auto [ptr, ec] = std::from_chars(start, end, value, 10);
		if (ec != std::errc() || ptr != end) {
			return std::nullopt;
		}
		return value;
	}

	CommandRegistry* registry;
	Database db;
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

TEST_F(StringFamilyTest, Type) {
	HashFamily::Register(registry);

	EXPECT_EQ(Execute("TYPE", {"missing"}), "+none\r\n");

	Execute("SET", {"str_key", "value"});
	EXPECT_EQ(Execute("TYPE", {"str_key"}), "+string\r\n");

	EXPECT_EQ(Execute("HSET", {"hash_key", "field", "value"}), "+OK\r\n");
	EXPECT_EQ(Execute("TYPE", {"hash_key"}), "+hash\r\n");
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

TEST_F(StringFamilyTest, ExpireTTLAndPersist) {
	Execute("SET", {"key", "value"});
	EXPECT_EQ(Execute("TTL", {"key"}), ":-1\r\n");

	EXPECT_EQ(Execute("EXPIRE", {"key", "10"}), ":1\r\n");
	auto ttl = ParseIntegerResponse(Execute("TTL", {"key"}));
	ASSERT_TRUE(ttl.has_value());
	EXPECT_GE(*ttl, 0);
	EXPECT_LE(*ttl, 10);

	EXPECT_EQ(Execute("PERSIST", {"key"}), ":1\r\n");
	EXPECT_EQ(Execute("TTL", {"key"}), ":-1\r\n");
	EXPECT_EQ(Execute("PERSIST", {"key"}), ":0\r\n");
}

TEST_F(StringFamilyTest, ExpireMissingAndImmediateExpiry) {
	EXPECT_EQ(Execute("EXPIRE", {"missing", "10"}), ":0\r\n");
	EXPECT_EQ(Execute("TTL", {"missing"}), ":-2\r\n");
	EXPECT_EQ(Execute("PERSIST", {"missing"}), ":0\r\n");

	Execute("SET", {"key", "value"});
	EXPECT_EQ(Execute("EXPIRE", {"key", "0"}), ":1\r\n");
	EXPECT_EQ(Execute("GET", {"key"}), "$-1\r\n");
	EXPECT_EQ(Execute("TTL", {"key"}), ":-2\r\n");
}

TEST_F(StringFamilyTest, SetWithEXAndPX) {
	EXPECT_EQ(Execute("SET", {"ex_key", "value", "EX", "1"}), "+OK\r\n");
	auto ex_ttl = ParseIntegerResponse(Execute("TTL", {"ex_key"}));
	ASSERT_TRUE(ex_ttl.has_value());
	EXPECT_GE(*ex_ttl, 0);
	EXPECT_LE(*ex_ttl, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(1100));
	EXPECT_EQ(Execute("GET", {"ex_key"}), "$-1\r\n");

	EXPECT_EQ(Execute("SET", {"px_key", "value", "PX", "20"}), "+OK\r\n");
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	EXPECT_EQ(Execute("GET", {"px_key"}), "$-1\r\n");
}

TEST_F(StringFamilyTest, SetExpireOptionErrors) {
	EXPECT_TRUE(Execute("SET", {"k", "v", "EX", "0"}).find("invalid expire time") != std::string::npos);
	EXPECT_TRUE(Execute("SET", {"k", "v", "PX", "-1"}).find("invalid expire time") != std::string::npos);
	EXPECT_TRUE(Execute("SET", {"k", "v", "BADOPT", "1"}).find("syntax error") != std::string::npos);
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

TEST_F(StringFamilyTest, IntegerParsingErrors) {
	Execute("SET", {"counter", "abc"});
	EXPECT_TRUE(Execute("INCR", {"counter"}).find("value is not an integer or out of range") != std::string::npos);
	EXPECT_TRUE(Execute("DECR", {"counter"}).find("value is not an integer or out of range") != std::string::npos);
	EXPECT_TRUE(Execute("INCRBY", {"counter", "1"}).find("value is not an integer or out of range") !=
	            std::string::npos);
	EXPECT_TRUE(Execute("DECRBY", {"counter", "1"}).find("value is not an integer or out of range") !=
	            std::string::npos);

	EXPECT_TRUE(Execute("INCRBY", {"counter", "x"}).find("value is not an integer or out of range") !=
	            std::string::npos);
	EXPECT_TRUE(Execute("DECRBY", {"counter", "x"}).find("value is not an integer or out of range") !=
	            std::string::npos);
	EXPECT_TRUE(Execute("GETRANGE", {"counter", "a", "1"}).find("value is not an integer or out of range") !=
	            std::string::npos);
	EXPECT_TRUE(Execute("SETRANGE", {"counter", "a", "x"}).find("value is not an integer or out of range") !=
	            std::string::npos);
	EXPECT_TRUE(Execute("SELECT", {"abc"}).find("value is not an integer or out of range") != std::string::npos);
}

TEST_F(StringFamilyTest, SelectUsesPerConnectionStateWhenPresent) {
	Connection conn1(nullptr);
	Connection conn2(nullptr);

	EXPECT_EQ(ExecuteWithConnection("SET", {"shared", "db0"}, &conn2), "+OK\r\n");

	EXPECT_EQ(ExecuteWithConnection("SELECT", {"1"}, &conn1), "+OK\r\n");
	EXPECT_EQ(conn1.GetDBIndex(), 1U);
	EXPECT_EQ(conn2.GetDBIndex(), 0U);

	EXPECT_EQ(ExecuteWithConnection("SET", {"shared", "db1"}, &conn1), "+OK\r\n");
	EXPECT_EQ(ExecuteWithConnection("GET", {"shared"}, &conn1), "$3\r\ndb1\r\n");
	EXPECT_EQ(ExecuteWithConnection("GET", {"shared"}, &conn2), "$3\r\ndb0\r\n");
}

TEST_F(StringFamilyTest, ErrorCases) {
	EXPECT_TRUE(Execute("SET", {}).find("wrong number") != std::string::npos);
	EXPECT_TRUE(Execute("GET", {}).find("wrong number") != std::string::npos);
	EXPECT_TRUE(Execute("INCR", {}).find("wrong number") != std::string::npos);
	EXPECT_TRUE(Execute("APPEND", {}).find("wrong number") != std::string::npos);
}
