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

template <typename IntT>
inline int ToChars(char* first, char* last, IntT value) {
	auto res = std::to_chars(first, last, value);
	if (res.ec != std::errc()) {
		return -1;
	}
	return static_cast<int>(res.ptr - first);
}
}

const std::string& RESPParser::ok_response() {
	return kOkResponse;
}

const std::string& RESPParser::pong_response() {
	return kPongResponse;
}

const std::string& RESPParser::null_bulk_response() {
	return kNullBulkResponse;
}

const std::string& RESPParser::empty_array_response() {
	return kEmptyArrayResponse;
}

int RESPParser::fill_buffer() {
    if (buffer_pos_ >= buffer_size_) {
        ssize_t n = stream_->recv(buffer_, sizeof(buffer_));
        if (n <= 0) {
            return -1;
        }
        buffer_size_ = n;
        buffer_pos_ = 0;
    }
    return 0;
}

char RESPParser::read_char() {
    if (fill_buffer() < 0) {
        return 0;
    }
    return buffer_[buffer_pos_++];
}

int RESPParser::ReadLineView(std::string_view* out) {
    scratch_line_.clear();
    bool used_scratch = false;

    while (true) {
        if (fill_buffer() < 0) {
            *out = std::string_view();
            return -1;
        }

        const char* start = buffer_ + buffer_pos_;
        const char* end = buffer_ + buffer_size_;
        const size_t avail = static_cast<size_t>(end - start);

        const char* lf = static_cast<const char*>(std::memchr(start, '\n', avail));
        const char* cr = static_cast<const char*>(std::memchr(start, '\r', avail));

        const char* term = nullptr;
        bool term_is_cr = false;
        if (lf && cr) {
            term = (lf < cr) ? lf : cr;
            term_is_cr = (term == cr);
        } else if (lf) {
            term = lf;
        } else if (cr) {
            term = cr;
            term_is_cr = true;
        }

        if (term == nullptr) {
            used_scratch = true;
            scratch_line_.append(start, avail);
            buffer_pos_ = buffer_size_;
            continue;
        }

        if (used_scratch) {
            scratch_line_.append(start, static_cast<size_t>(term - start));
            *out = std::string_view(scratch_line_);
        } else {
            *out = std::string_view(start, static_cast<size_t>(term - start));
        }

        buffer_pos_ = static_cast<size_t>((term - buffer_) + 1);
        if (term_is_cr && buffer_pos_ < buffer_size_ && buffer_[buffer_pos_] == '\n') {
            buffer_pos_++;
        }
        return 0;
    }
}

int RESPParser::ReadInlineLineView(char first_char, std::string_view* out) {
    // `first_char` was read from buffer_[buffer_pos_ - 1].
    // Fast path: if the line terminator is in the current buffer, return a view into buffer_.
    // Slow path: if the line spans buffers, stitch into scratch_inline_ (reused).
    const char* first_ptr = (buffer_pos_ > 0) ? (buffer_ + buffer_pos_ - 1) : nullptr;
    scratch_inline_.clear();
    bool used_scratch = false;

    while (true) {
        if (fill_buffer() < 0) {
            *out = std::string_view();
            return -1;
        }

        const char* start = buffer_ + buffer_pos_;
        const char* end = buffer_ + buffer_size_;
        const size_t avail = static_cast<size_t>(end - start);

        const char* lf = static_cast<const char*>(std::memchr(start, '\n', avail));
        const char* cr = static_cast<const char*>(std::memchr(start, '\r', avail));

        const char* term = nullptr;
        bool term_is_cr = false;
        if (lf && cr) {
            term = (lf < cr) ? lf : cr;
            term_is_cr = (term == cr);
        } else if (lf) {
            term = lf;
        } else if (cr) {
            term = cr;
            term_is_cr = true;
        }

        if (term == nullptr) {
            if (!used_scratch) {
                used_scratch = true;
                scratch_inline_.push_back(first_char);
            }
            scratch_inline_.append(start, avail);
            buffer_pos_ = buffer_size_;
            continue;
        }

        if (used_scratch) {
            scratch_inline_.append(start, static_cast<size_t>(term - start));
            *out = std::string_view(scratch_inline_);
        } else {
            // If we couldn't compute a pointer to the first char, fall back to scratch.
            if (first_ptr == nullptr) {
                scratch_inline_.push_back(first_char);
                scratch_inline_.append(start, static_cast<size_t>(term - start));
                *out = std::string_view(scratch_inline_);
            } else {
                *out = std::string_view(first_ptr, static_cast<size_t>(term - first_ptr));
            }
        }

        buffer_pos_ = static_cast<size_t>((term - buffer_) + 1);
        if (term_is_cr && buffer_pos_ < buffer_size_ && buffer_[buffer_pos_] == '\n') {
            buffer_pos_++;
        }
        return 0;
    }
}

std::string RESPParser::read_line() {
    std::string_view sv;
    if (ReadLineView(&sv) < 0) {
        return {};
    }
    return std::string(sv);
}

std::string RESPParser::read_bulk_string(int64_t len) {
    if (len < 0) {
        return "";
    }
    std::string result;
    result.reserve(len);

    size_t total_read = 0;
    while (total_read < (size_t)len) {
        if (fill_buffer() < 0) {
            return "";
        }
        size_t available = buffer_size_ - buffer_pos_;
        size_t to_read = std::min(available, (size_t)len - total_read);
        result.append(buffer_ + buffer_pos_, to_read);
        buffer_pos_ += to_read;
        total_read += to_read;
    }

    read_char();
    read_char();

    return result;
}

int RESPParser::read_bulk_string_into(int64_t len, NanoObj& out) {
    if (len < 0) {
        out = NanoObj();
        return 0;
    }

    char* dst = out.PrepareStringBuffer(static_cast<size_t>(len));
    size_t total_read = 0;
    while (total_read < static_cast<size_t>(len)) {
        if (fill_buffer() < 0) {
            out = NanoObj();
            return -1;
        }
        size_t available = buffer_size_ - buffer_pos_;
        size_t to_read = std::min(available, static_cast<size_t>(len) - total_read);
        std::memcpy(dst + total_read, buffer_ + buffer_pos_, to_read);
        buffer_pos_ += to_read;
        total_read += to_read;
    }

    read_char();
    read_char();

    out.FinalizePreparedString();
    out.MaybeConvertToInt();
    return 0;
}

int RESPParser::parse_inline_command(std::string_view line, std::vector<NanoObj>& args) {
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
            args.push_back(NanoObj::fromKey(std::string_view(token_start, p - token_start)));
        }
    }

    return args.size() > 0 ? 0 : -1;
}

int RESPParser::parse_array(std::vector<NanoObj>& args) {
    std::string_view line;
    if (ReadLineView(&line) < 0 || line.empty()) {
        return -1;
    }

    int64_t count = 0;
    if (!string2ll(line.data(), line.size(), &count)) {
        return -1;
    }
    if (count < 0) {
        return 0;
    }

    if (count > 0 && count <= static_cast<int64_t>(std::numeric_limits<size_t>::max())) {
        args.reserve(static_cast<size_t>(count));
    }

    for (int64_t i = 0; i < count; i++) {
        char c = read_char();
        if (c == '\r' || c == '\n') {
            c = read_char();
        }

        if (c == '$') {
            std::string_view len_str;
            if (ReadLineView(&len_str) < 0) {
                return -1;
            }
            int64_t len = 0;
            if (!string2ll(len_str.data(), len_str.size(), &len)) {
                return -1;
            }
            NanoObj bulk;
            if (read_bulk_string_into(len, bulk) < 0) {
                return -1;
            }
            args.push_back(std::move(bulk));
        } else if (c == '+') {
            std::string_view sv;
            if (ReadLineView(&sv) < 0) {
                return -1;
            }
            args.push_back(NanoObj::fromKey(sv));
        } else if (c == ':') {
            std::string_view sv;
            if (ReadLineView(&sv) < 0) {
                return -1;
            }
            args.push_back(NanoObj::fromKey(sv));
        } else if (c == '-') {
            std::string_view sv;
            if (ReadLineView(&sv) < 0) {
                return -1;
            }
            args.push_back(NanoObj::fromKey(sv));
        } else {
            return -1;
        }
    }

    return args.size();
}

int RESPParser::parse_value(ParsedValue& value) {
    char c = read_char();
    if (c == '\r' || c == '\n') {
        c = read_char();
    }

    switch (c) {
        case '+':
            value.type = DataType::SimpleString;
            {
                std::string_view sv;
                if (ReadLineView(&sv) < 0) {
                    return -1;
                }
                value.obj_value = NanoObj::fromKey(sv);
            }
            return 0;
        case '-':
            value.type = DataType::Error;
            {
                std::string_view sv;
                if (ReadLineView(&sv) < 0) {
                    return -1;
                }
                value.obj_value = NanoObj::fromKey(sv);
            }
            return 0;
        case ':': {
            value.type = DataType::Integer;
            std::string_view line;
            if (ReadLineView(&line) < 0) {
                return -1;
            }
            int64_t int_val = 0;
            if (!string2ll(line.data(), line.size(), &int_val)) {
                return -1;
            }
            value.obj_value = NanoObj::fromInt(int_val);
            return 0;
        }
        case '$': {
            std::string_view len_str;
            if (ReadLineView(&len_str) < 0) {
                return -1;
            }
            int64_t len = 0;
            if (!string2ll(len_str.data(), len_str.size(), &len)) {
                return -1;
            }
            value.type = DataType::BulkString;
            if (len >= 0) {
                if (read_bulk_string_into(len, value.obj_value) < 0) {
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

int RESPParser::parse_command(std::vector<NanoObj>& args) {
    args.clear();

    char c = read_char();
    if (c == 0) {
        return -1;
    }

    if (c == '*') {
        return parse_array(args);
    } else {
        std::string_view line;
        if (ReadInlineLineView(c, &line) < 0) {
            return -1;
        }
        return parse_inline_command(line, args);
    }
}

std::string RESPParser::make_simple_string(const std::string& s) {
    std::string result;
    result.reserve(1 + s.size() + 2);
    result.push_back('+');
    result.append(s);
    result.append("\r\n",2);
    return result;
}

std::string RESPParser::make_error(const std::string& msg) {
    std::string result;
    result.reserve(5 + msg.size() + 2);
    result.append("-ERR ", 5);
    result.append(msg);
    result.append("\r\n",2);
    return result;
}

std::string RESPParser::make_bulk_string(const std::string& s) {
    char len_buf[24];
	int len_len = ToChars(len_buf, len_buf + sizeof(len_buf), s.size());
	if (len_len < 0) {
		return "$-1\r\n";
	}
    std::string result;
    result.reserve(1 + len_len + 2 + s.size() + 2);
    result.push_back('$');
    result.append(len_buf, len_len);
    result.append("\r\n",2);
    result.append(s);
    result.append("\r\n",2);
    return result;
}

std::string RESPParser::make_null_bulk_string() {
    return "$-1\r\n";
}

std::string RESPParser::make_integer(int64_t value) {
    char buf[24];
	int len = ToChars(buf, buf + sizeof(buf), value);
	if (len < 0) {
		return ":0\r\n";
	}
    std::string result;
    result.reserve(1 + len + 2);
    result.push_back(':');
    result.append(buf, len);
    result.append("\r\n",2);
    return result;
}

std::string RESPParser::make_array(int64_t count) {
    char buf[24];
	int len = ToChars(buf, buf + sizeof(buf), count);
	if (len < 0) {
		return "*0\r\n";
	}
    std::string result;
    result.reserve(1 + len + 2);
    result.push_back('*');
    result.append(buf, len);
    result.append("\r\n",2);
    return result;
}
