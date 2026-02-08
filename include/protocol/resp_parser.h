#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <photon/net/socket.h>
#include "core/nano_obj.h"

class RESPParser {
public:
	enum class DataType { SIMPLE_STRING, ERROR, INTEGER, BULK_STRING, ARRAY, INLINE_COMMAND };

	struct ParsedValue {
		DataType type;
		NanoObj obj_value;
		std::vector<ParsedValue> array_value;

		ParsedValue() : type(DataType::SIMPLE_STRING), obj_value() {
		}
	};

	explicit RESPParser(photon::net::ISocketStream* stream) : stream(stream) {
	}

	int ParseCommand(std::vector<NanoObj>& args);

	// NOLINTNEXTLINE(readability-identifier-naming)
	int parse_command(std::vector<NanoObj>& args) {
		return ParseCommand(args);
	}

	static const std::string& OkResponse();
	static const std::string& PongResponse();
	static const std::string& NullBulkResponse();
	static const std::string& EmptyArrayResponse();

	// Backward-compatible wrappers (transitional).
	// NOLINTNEXTLINE(readability-identifier-naming)
	static const std::string& ok_response() {
		return OkResponse();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static const std::string& pong_response() {
		return PongResponse();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static const std::string& null_bulk_response() {
		return NullBulkResponse();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static const std::string& empty_array_response() {
		return EmptyArrayResponse();
	}

	static std::string MakeSimpleString(const std::string& s);
	static std::string MakeError(const std::string& msg);
	static std::string MakeBulkString(const std::string& s);
	static std::string MakeNullBulkString();
	static std::string MakeInteger(int64_t value);
	static std::string MakeArray(int64_t count);

	// NOLINTNEXTLINE(readability-identifier-naming)
	static std::string make_simple_string(const std::string& s) {
		return MakeSimpleString(s);
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static std::string make_error(const std::string& msg) {
		return MakeError(msg);
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static std::string make_bulk_string(const std::string& s) {
		return MakeBulkString(s);
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static std::string make_null_bulk_string() {
		return MakeNullBulkString();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static std::string make_integer(int64_t value) {
		return MakeInteger(value);
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static std::string make_array(int64_t count) {
		return MakeArray(count);
	}

private:
	int ParseArray(std::vector<NanoObj>& args);
	int ParseInlineCommand(std::string_view line, std::vector<NanoObj>& args);
	int ParseValue(ParsedValue& value);

	char ReadChar();
	std::string ReadLine();
	std::string ReadBulkString(int64_t len);
	int ReadBulkStringInto(int64_t len, NanoObj& out);
	int FillBuffer();

private:
	int ReadLineView(std::string_view* out);
	int ReadInlineLineView(char first_char, std::string_view* out);
	void ConsumeCRLF(bool term_is_cr);

	// Scratch buffers used when the line spans multiple recv() fills.
	// These are reused across calls to avoid per-command allocations.
	std::string scratch_line;
	std::string scratch_inline;

	photon::net::ISocketStream* stream;
	char buffer[8192];
	size_t buffer_pos = 0;
	size_t buffer_size = 0;
};
