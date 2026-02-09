#include <gtest/gtest.h>
#include "protocol/resp_parser.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

namespace {

class FakeSocketStream final : public photon::net::ISocketStream {
public:
	explicit FakeSocketStream(std::string data, std::vector<size_t> chunk_sizes = {})
	    : data(std::move(data)), chunk_sizes(std::move(chunk_sizes)) {
	}

public:
	Object* get_underlay_object(uint64_t recursion = 0) override {
		(void)recursion;
		return nullptr;
	}

	int setsockopt(int level, int option_name, const void* option_value, socklen_t option_len) override {
		(void)level;
		(void)option_name;
		(void)option_value;
		(void)option_len;
		errno = ENOSYS;
		return -1;
	}

	int getsockopt(int level, int option_name, void* option_value, socklen_t* option_len) override {
		(void)level;
		(void)option_name;
		(void)option_value;
		(void)option_len;
		errno = ENOSYS;
		return -1;
	}

	int getsockname(photon::net::EndPoint& addr) override {
		addr.clear();
		errno = ENOSYS;
		return -1;
	}

	int getpeername(photon::net::EndPoint& addr) override {
		addr.clear();
		errno = ENOSYS;
		return -1;
	}

	int getsockname(char* path, size_t count) override {
		(void)path;
		(void)count;
		errno = ENOSYS;
		return -1;
	}

	int getpeername(char* path, size_t count) override {
		(void)path;
		(void)count;
		errno = ENOSYS;
		return -1;
	}

	int close() override {
		return 0;
	}

	ssize_t read(void* buf, size_t count) override {
		return recv(buf, count, 0);
	}

	ssize_t readv(const struct iovec* iov, int iovcnt) override {
		return recv(iov, iovcnt, 0);
	}

	ssize_t write(const void* buf, size_t count) override {
		return send(buf, count, 0);
	}

	ssize_t writev(const struct iovec* iov, int iovcnt) override {
		return send(iov, iovcnt, 0);
	}

	ssize_t recv(void* buf, size_t count, int flags = 0) override {
		(void)flags;
		if (pos >= data.size()) {
			return 0;
		}

		size_t chunk_limit = count;
		if (chunk_index < chunk_sizes.size()) {
			chunk_limit = chunk_sizes[chunk_index];
		}
		++chunk_index;

		const size_t remaining = data.size() - pos;
		const size_t n = std::min({count, remaining, chunk_limit});
		std::memcpy(buf, data.data() + pos, n);
		pos += n;
		return static_cast<ssize_t>(n);
	}

	ssize_t recv(const struct iovec* iov, int iovcnt, int flags = 0) override {
		(void)flags;
		if (iovcnt <= 0) {
			return 0;
		}
		ssize_t total = 0;
		for (int i = 0; i < iovcnt; ++i) {
			if (iov[i].iov_len == 0) {
				continue;
			}
			ssize_t n = recv(iov[i].iov_base, iov[i].iov_len, flags);
			if (n <= 0) {
				return total == 0 ? n : total;
			}
			total += n;
			if (static_cast<size_t>(n) < iov[i].iov_len) {
				break;
			}
		}
		return total;
	}

	ssize_t send(const void* buf, size_t count, int flags = 0) override {
		(void)buf;
		(void)count;
		(void)flags;
		errno = ENOSYS;
		return -1;
	}

	ssize_t send(const struct iovec* iov, int iovcnt, int flags = 0) override {
		(void)iov;
		(void)iovcnt;
		(void)flags;
		errno = ENOSYS;
		return -1;
	}

	ssize_t sendfile(int in_fd, off_t offset, size_t count) override {
		(void)in_fd;
		(void)offset;
		(void)count;
		errno = ENOSYS;
		return -1;
	}

private:
	std::string data;
	std::vector<size_t> chunk_sizes;
	size_t pos = 0;
	size_t chunk_index = 0;
};

} // namespace

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

TEST(RESPParserTest, ParseInlineCommand_LFOnly) {
	FakeSocketStream stream("PING\n");
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_GE(ret, 0);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");
}

TEST(RESPParserTest, ParseInlineCommand_SpansRecvBuffers) {
	// Force the inline line to span multiple recv() calls so the parser must use scratch_inline_.
	FakeSocketStream stream("PING\r\n", {1, 1, 1, 1, 1, 1});
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_GE(ret, 0);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");
}

TEST(RESPParserTest, ParseInlineCommand_CROnly) {
	// Not standard RESP, but current implementation treats CR as a terminator.
	FakeSocketStream stream("PING\r");
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_GE(ret, 0);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");
}

TEST(RESPParserTest, ParseArrayWithBulkStrings_SpansRecvBuffers) {
	const std::string cmd = "*2\r\n$4\r\nECHO\r\n$12\r\nhello world!\r\n";
	// Split into many small chunks to exercise fill_buffer/read_bulk_string_into paths.
	FakeSocketStream stream(cmd, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2});
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_EQ(ret, 2);
	ASSERT_EQ(args.size(), 2U);
	EXPECT_EQ(args[0].ToString(), "ECHO");
	EXPECT_EQ(args[1].ToString(), "hello world!");
}

TEST(RESPParserTest, HasBufferedDataForPipelinedCommands) {
	const std::string pipelined = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPONG\r\n";
	FakeSocketStream stream(pipelined, {pipelined.size()});
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");
	EXPECT_TRUE(parser.has_buffered_data());

	ret = parser.parse_command(args);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PONG");
	EXPECT_FALSE(parser.has_buffered_data());
}

TEST(RESPParserTest, TryParseCommandNoReadParsesCompleteBufferedCommand) {
	const std::string pipelined = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPONG\r\n";
	FakeSocketStream stream(pipelined, {pipelined.size()});
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");

	auto status = parser.try_parse_command_no_read(args);
	EXPECT_EQ(status, RESPParser::TryParseResult::OK);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PONG");

	status = parser.try_parse_command_no_read(args);
	EXPECT_EQ(status, RESPParser::TryParseResult::NEED_MORE);
	EXPECT_TRUE(args.empty());
}

TEST(RESPParserTest, TryParseCommandNoReadNeedMoreKeepsPartialCommandBuffered) {
	const std::string request = "*1\r\n$4\r\nPING\r\n*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n";
	FakeSocketStream stream(request, {request.size()});
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");
	EXPECT_TRUE(parser.has_buffered_data());

	auto status = parser.try_parse_command_no_read(args);
	EXPECT_EQ(status, RESPParser::TryParseResult::NEED_MORE);
	EXPECT_TRUE(args.empty());
	EXPECT_TRUE(parser.has_buffered_data());

	status = parser.try_parse_command_no_read(args);
	EXPECT_EQ(status, RESPParser::TryParseResult::NEED_MORE);
	EXPECT_TRUE(args.empty());
	EXPECT_TRUE(parser.has_buffered_data());
}

TEST(RESPParserTest, TryParseCommandNoReadReportsMalformedBufferedCommand) {
	const std::string request = "*1\r\n$4\r\nPING\r\n*X\r\n";
	FakeSocketStream stream(request, {request.size()});
	RESPParser parser(&stream);

	std::vector<NanoObj> args;
	int ret = parser.parse_command(args);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(args.size(), 1U);
	EXPECT_EQ(args[0].ToString(), "PING");

	auto status = parser.try_parse_command_no_read(args);
	EXPECT_EQ(status, RESPParser::TryParseResult::ERROR);
	EXPECT_TRUE(args.empty());
}
