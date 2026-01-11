#pragma once

#include <string>
#include <vector>
#include <photon/net/socket.h>
#include "core/compact_obj.h"

class RESPParser {
public:
	enum class DataType { SimpleString, Error, Integer, BulkString, Array, InlineCommand };

	struct ParsedValue {
		DataType type;
		CompactObj obj_value;
		std::vector<ParsedValue> array_value;

		ParsedValue() : type(DataType::SimpleString), obj_value() {
		}
	};

	RESPParser(photon::net::ISocketStream* stream) : stream_(stream) {
	}

	int parse_command(std::vector<CompactObj>& args);

	// Pre-computed common responses for hot paths
	static const std::string& ok_response();
	static const std::string& pong_response();
	static const std::string& null_bulk_response();
	static const std::string& empty_array_response();

	static std::string make_simple_string(const std::string& s);
	static std::string make_error(const std::string& msg);
	static std::string make_bulk_string(const std::string& s);
	static std::string make_null_bulk_string();
	static std::string make_integer(int64_t value);
	static std::string make_array(int64_t count);

private:
	int parse_array(std::vector<CompactObj>& args);
	int parse_inline_command(const std::string& line, std::vector<CompactObj>& args);
	int parse_value(ParsedValue& value);

	char read_char();
	std::string read_line();
	std::string read_bulk_string(int64_t len);
	int fill_buffer();

private:
	photon::net::ISocketStream* stream_;
	char buffer_[1024];
	size_t buffer_pos_ = 0;
	size_t buffer_size_ = 0;
};
