#include "protocol/resp_parser.h"
#include "core/util.h"
#include <photon/common/alog.h>
#include <photon/net/socket.h>
#include <charconv>
#include <cctype>
#include <cstring>
#include <limits>
#include <system_error>

namespace {

const std::string kOkResponse = "+OK\r\n";
const std::string kPongResponse = "+PONG\r\n";
const std::string kNullBulkResponse = "$-1\r\n";
const std::string kEmptyArrayResponse = "*0\r\n";

template <typename INT_T>
inline int ToChars(char* first, char* last, INT_T value) {
	auto res = std::to_chars(first, last, value);
	return res.ec == std::errc() ? static_cast<int>(res.ptr - first) : -1;
}

// Find first CR or LF in [start, start+avail). Returns {terminator_ptr, is_cr}.
inline std::pair<const char*, bool> FindTerminator(const char* start, size_t avail) {
	const char* lf = static_cast<const char*>(std::memchr(start, '\n', avail));
	const char* cr = static_cast<const char*>(std::memchr(start, '\r', avail));
	if (lf && cr) {
		return (lf < cr) ? std::make_pair(lf, false) : std::make_pair(cr, true);
	}
	if (lf) {
		return {lf, false};
	}
	if (cr) {
		return {cr, true};
	}
	return {nullptr, false};
}

} // namespace

const std::string& RESPParser::OkResponse() {
	return kOkResponse;
}
const std::string& RESPParser::PongResponse() {
	return kPongResponse;
}
const std::string& RESPParser::NullBulkResponse() {
	return kNullBulkResponse;
}
const std::string& RESPParser::EmptyArrayResponse() {
	return kEmptyArrayResponse;
}

int RESPParser::FillBuffer() {
	if (buffer_pos >= buffer_size) {
		if (!allow_socket_read) {
			no_read_need_more = true;
			return -1;
		}
		ssize_t n = stream->recv(buffer, sizeof(buffer));
		if (n <= 0) {
			return -1;
		}
		buffer_size = n;
		buffer_pos = 0;
	}
	return 0;
}

char RESPParser::ReadChar() {
	return FillBuffer() < 0 ? '\0' : buffer[buffer_pos++];
}

void RESPParser::ConsumeCRLF(bool term_is_cr) {
	if (!term_is_cr) {
		return;
	}
	// Handle CRLF across recv() boundaries
	if (buffer_pos >= buffer_size) {
		(void)FillBuffer();
	}
	if (buffer_pos < buffer_size && buffer[buffer_pos] == '\n') {
		buffer_pos++;
	}
}

int RESPParser::ReadLineView(std::string_view* out) {
	scratch_line.clear();
	bool used_scratch = false;

	for (;;) {
		if (FillBuffer() < 0) {
			*out = {};
			return -1;
		}
		const char* start = buffer + buffer_pos;
		size_t avail = buffer_size - buffer_pos;
		auto [term, is_cr] = FindTerminator(start, avail);

		if (!term) {
			used_scratch = true;
			scratch_line.append(start, avail);
			buffer_pos = buffer_size;
			continue;
		}

		size_t seg_len = static_cast<size_t>(term - start);
		if (used_scratch) {
			scratch_line.append(start, seg_len);
			*out = scratch_line;
		} else {
			*out = std::string_view(start, seg_len);
		}
		buffer_pos = static_cast<size_t>((term - buffer) + 1);
		ConsumeCRLF(is_cr);
		return 0;
	}
}

int RESPParser::ReadInlineLineView(char first_char, std::string_view* out) {
	const char* first_ptr = (buffer_pos > 0) ? (buffer + buffer_pos - 1) : nullptr;
	scratch_inline.clear();
	bool used_scratch = false;

	for (;;) {
		if (FillBuffer() < 0) {
			*out = {};
			return -1;
		}
		const char* start = buffer + buffer_pos;
		size_t avail = buffer_size - buffer_pos;
		auto [term, is_cr] = FindTerminator(start, avail);

		if (!term) {
			if (!used_scratch) {
				used_scratch = true;
				scratch_inline.push_back(first_char);
			}
			scratch_inline.append(start, avail);
			buffer_pos = buffer_size;
			continue;
		}

		size_t seg_len = static_cast<size_t>(term - start);
		if (used_scratch) {
			scratch_inline.append(start, seg_len);
			*out = scratch_inline;
		} else if (first_ptr) {
			*out = std::string_view(first_ptr, static_cast<size_t>(term - first_ptr));
		} else {
			scratch_inline.push_back(first_char);
			scratch_inline.append(start, seg_len);
			*out = scratch_inline;
		}
		buffer_pos = static_cast<size_t>((term - buffer) + 1);
		ConsumeCRLF(is_cr);
		return 0;
	}
}

std::string RESPParser::ReadLine() {
	std::string_view sv;
	return ReadLineView(&sv) < 0 ? std::string {} : std::string(sv);
}

std::string RESPParser::ReadBulkString(int64_t len) {
	if (len < 0) {
		return "";
	}
	std::string result;
	result.reserve(len);
	for (size_t total = 0; total < static_cast<size_t>(len);) {
		if (FillBuffer() < 0) {
			return "";
		}
		size_t chunk = std::min(buffer_size - buffer_pos, static_cast<size_t>(len) - total);
		result.append(buffer + buffer_pos, chunk);
		buffer_pos += chunk;
		total += chunk;
	}
	ReadChar();
	ReadChar(); // consume trailing CRLF
	return result;
}

int RESPParser::ReadBulkStringInto(int64_t len, NanoObj& out) {
	if (len < 0) {
		out = NanoObj();
		return 0;
	}
	char* dst = out.PrepareStringBuffer(static_cast<size_t>(len));
	for (size_t total = 0; total < static_cast<size_t>(len);) {
		if (FillBuffer() < 0) {
			out = NanoObj();
			return -1;
		}
		size_t chunk = std::min(buffer_size - buffer_pos, static_cast<size_t>(len) - total);
		std::memcpy(dst + total, buffer + buffer_pos, chunk);
		buffer_pos += chunk;
		total += chunk;
	}
	ReadChar();
	ReadChar(); // consume trailing CRLF
	out.FinalizePreparedString();
	out.MaybeConvertToInt();
	return 0;
}

int RESPParser::ParseInlineCommand(std::string_view line, std::vector<NanoObj>& args) {
	const char* p = line.data();
	const char* end = p + line.size();
	while (p < end) {
		while (p < end && (*p == ' ' || *p == '\t')) {
			++p;
		}
		if (p >= end) {
			break;
		}
		const char* token_start = p;
		while (p < end && *p != ' ' && *p != '\t') {
			++p;
		}
		if (p > token_start) {
			args.push_back(NanoObj::FromKey(std::string_view(token_start, p - token_start)));
		}
	}
	return args.empty() ? -1 : 0;
}

int RESPParser::ParseArray(std::vector<NanoObj>& args) {
	std::string_view line;
	if (ReadLineView(&line) < 0 || line.empty()) {
		return -1;
	}

	int64_t count = 0;
	if (!String2ll(line.data(), line.size(), &count)) {
		return -1;
	}
	if (count < 0) {
		return 0;
	}
	if (count > 0) {
		args.reserve(static_cast<size_t>(count));
	}

	for (int64_t i = 0; i < count; ++i) {
		char c = ReadChar();
		if (c == '\r' || c == '\n') {
			c = ReadChar();
		}

		if (c == '$') {
			std::string_view len_str;
			if (ReadLineView(&len_str) < 0) {
				return -1;
			}
			int64_t len = 0;
			if (!String2ll(len_str.data(), len_str.size(), &len)) {
				return -1;
			}
			NanoObj bulk;
			if (ReadBulkStringInto(len, bulk) < 0) {
				return -1;
			}
			args.push_back(std::move(bulk));
		} else if (c == '+' || c == ':' || c == '-') {
			std::string_view sv;
			if (ReadLineView(&sv) < 0) {
				return -1;
			}
			args.push_back(NanoObj::FromKey(sv));
		} else {
			return -1;
		}
	}
	return static_cast<int>(args.size());
}

int RESPParser::ParseValue(ParsedValue& value) {
	char c = ReadChar();
	if (c == '\r' || c == '\n') {
		c = ReadChar();
	}

	std::string_view sv;
	switch (c) {
	case '+':
		value.type = DataType::SIMPLE_STRING;
		if (ReadLineView(&sv) < 0) {
			return -1;
		}
		value.obj_value = NanoObj::FromKey(sv);
		return 0;
	case '-':
		value.type = DataType::ERROR;
		if (ReadLineView(&sv) < 0) {
			return -1;
		}
		value.obj_value = NanoObj::FromKey(sv);
		return 0;
	case ':':
		value.type = DataType::INTEGER;
		if (ReadLineView(&sv) < 0) {
			return -1;
		}
		{
			int64_t v;
			if (!String2ll(sv.data(), sv.size(), &v)) {
				return -1;
			}
			value.obj_value = NanoObj::FromInt(v);
		}
		return 0;
	case '$': {
		if (ReadLineView(&sv) < 0) {
			return -1;
		}
		int64_t len;
		if (!String2ll(sv.data(), sv.size(), &len)) {
			return -1;
		}
		value.type = DataType::BULK_STRING;
		if (len >= 0) {
			if (ReadBulkStringInto(len, value.obj_value) < 0) {
				return -1;
			}
		} else {
			value.obj_value = NanoObj();
		}
		return 0;
	}
	default:
		return -1;
	}
}

int RESPParser::ParseCommand(std::vector<NanoObj>& args) {
	no_read_need_more = false;
	args.clear();
	char c = ReadChar();
	if (c == '\0') {
		return -1;
	}
	if (c == '*') {
		return ParseArray(args);
	}
	std::string_view line;
	if (ReadInlineLineView(c, &line) < 0) {
		return -1;
	}
	return ParseInlineCommand(line, args);
}

RESPParser::TryParseResult RESPParser::TryParseCommandNoRead(std::vector<NanoObj>& args) {
	const size_t saved_buffer_pos = buffer_pos;
	const size_t saved_buffer_size = buffer_size;
	const bool saved_allow_socket_read = allow_socket_read;

	allow_socket_read = false;
	int ret = ParseCommand(args);
	const bool need_more_data = no_read_need_more;
	allow_socket_read = saved_allow_socket_read;

	if (ret >= 0) {
		return TryParseResult::OK;
	}

	buffer_pos = saved_buffer_pos;
	buffer_size = saved_buffer_size;
	args.clear();
	return need_more_data ? TryParseResult::NEED_MORE : TryParseResult::ERROR;
}

// --- Response Builders ---

std::string RESPParser::MakeSimpleString(const std::string& s) {
	std::string r;
	r.reserve(1 + s.size() + 2);
	r += '+';
	r += s;
	r += "\r\n";
	return r;
}

std::string RESPParser::MakeError(const std::string& msg) {
	std::string r;
	r.reserve(5 + msg.size() + 2);
	r += "-ERR ";
	r += msg;
	r += "\r\n";
	return r;
}

std::string RESPParser::MakeBulkString(const std::string& s) {
	char buf[24];
	int n = ToChars(buf, buf + sizeof(buf), s.size());
	if (n < 0) {
		return "$-1\r\n";
	}
	std::string r;
	r.reserve(1 + n + 2 + s.size() + 2);
	r += '$';
	r.append(buf, n);
	r += "\r\n";
	r += s;
	r += "\r\n";
	return r;
}

std::string RESPParser::MakeNullBulkString() {
	return "$-1\r\n";
}

std::string RESPParser::MakeInteger(int64_t value) {
	char buf[24];
	int n = ToChars(buf, buf + sizeof(buf), value);
	if (n < 0) {
		return ":0\r\n";
	}
	std::string r;
	r.reserve(1 + n + 2);
	r += ':';
	r.append(buf, n);
	r += "\r\n";
	return r;
}

std::string RESPParser::MakeArray(int64_t count) {
	char buf[24];
	int n = ToChars(buf, buf + sizeof(buf), count);
	if (n < 0) {
		return "*0\r\n";
	}
	std::string r;
	r.reserve(1 + n + 2);
	r += '*';
	r.append(buf, n);
	r += "\r\n";
	return r;
}
