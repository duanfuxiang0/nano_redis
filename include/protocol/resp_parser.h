#pragma once

#include <string>
#include <vector>
#include <photon/common/stream.h>

class RESPParser {
public:
	enum class DataType { SimpleString, Error, Integer, BulkString, Array, InlineCommand };

	struct ParsedValue {
		DataType type;
		std::string str_value;
		int64_t int_value;
		std::vector<ParsedValue> array_value;

		ParsedValue() : type(DataType::SimpleString), int_value(0) {
		}
	};

	RESPParser(IStream* stream) : stream_(stream) {
	}

	int parse_command(std::vector<std::string>& args);

	static std::string make_simple_string(const std::string& s);
	static std::string make_error(const std::string& msg);
	static std::string make_bulk_string(const std::string& s);
	static std::string make_null_bulk_string();
	static std::string make_integer(int64_t value);
	static std::string make_array(int64_t count);

private:
	IStream* stream_;
	char buffer_[1024];
	size_t buffer_pos_ = 0;
	size_t buffer_size_ = 0;

	int fill_buffer();
	char read_char();
	std::string read_line();
	std::string read_bulk_string(int64_t len);
	int parse_array(std::vector<std::string>& args);
	int parse_inline_command(const std::string& line, std::vector<std::string>& args);
	int parse_value(ParsedValue& value);
};
