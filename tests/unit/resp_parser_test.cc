#include <gtest/gtest.h>
#include "protocol/resp_parser.h"
#include <string>
#include <vector>

using namespace testing;

TEST(RESPParserTest, RespBuilder_SimpleString) {
    std::string result = RESPParser::make_simple_string("OK");
    EXPECT_EQ("+OK\r\n", result);
}

TEST(RESPParserTest, RespBuilder_Error) {
    std::string result = RESPParser::make_error("unknown command");
    EXPECT_EQ("-ERR unknown command\r\n", result);
}

TEST(RESPParserTest, RespBuilder_BulkString) {
    std::string result = RESPParser::make_bulk_string("hello");
    EXPECT_EQ("$5\r\nhello\r\n", result);
}

TEST(RESPParserTest, RespBuilder_NullBulkString) {
    std::string result = RESPParser::make_null_bulk_string();
    EXPECT_EQ("$-1\r\n", result);
}

TEST(RESPParserTest, RespBuilder_Integer) {
    std::string result = RESPParser::make_integer(123);
    EXPECT_EQ(":123\r\n", result);
}

TEST(RESPParserTest, RespBuilder_Array) {
    std::string result = RESPParser::make_array(3);
    EXPECT_EQ("*3\r\n", result);
}
