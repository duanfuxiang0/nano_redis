#include "protocol/resp_parser.h"
#include "core/util.h"
#include <photon/common/alog.h>
#include <photon/net/socket.h>
#include <cctype>
#include <sstream>

// Pre-computed static responses for hot paths
namespace {
const std::string kOkResponse = "+OK\r\n";
const std::string kPongResponse = "+PONG\r\n";
const std::string kNullBulkResponse = "$-1\r\n";
const std::string kEmptyArrayResponse = "*0\r\n";
}  // namespace

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

std::string RESPParser::read_line() {
    std::string line;
    while (true) {
        if (fill_buffer() < 0) {
            break;
        }
        while (buffer_pos_ < buffer_size_) {
            char c = buffer_[buffer_pos_++];
            if (c == '\r') {
                if (buffer_pos_ < buffer_size_ && buffer_[buffer_pos_] == '\n') {
                    buffer_pos_++;
                    return line;
                } else {
                    line += c;
                }
            } else if (c == '\n') {
                return line;
            } else {
                line += c;
            }
        }
    }
    return line;
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

int RESPParser::parse_inline_command(const std::string& line, std::vector<CompactObj>& args) {
    std::istringstream iss(line);
    std::string arg;

    while (iss >> arg) {
        args.push_back(CompactObj::fromKey(arg));
    }

    return args.size() > 0 ? 0 : -1;
}

int RESPParser::parse_array(std::vector<CompactObj>& args) {
    std::string line = read_line();
    if (line.empty()) {
        return -1;
    }

    int64_t count = 0;
    if (!string2ll(line.data(), line.size(), &count)) {
        return -1;
    }
    if (count < 0) {
        return 0;
    }

    for (int64_t i = 0; i < count; i++) {
        char c = read_char();
        if (c == '\r' || c == '\n') {
            c = read_char();
        }

        if (c == '$') {
            std::string len_str = read_line();
            int64_t len = 0;
            if (!string2ll(len_str.data(), len_str.size(), &len)) {
                return -1;
            }
            std::string bulk = read_bulk_string(len);
            args.push_back(CompactObj::fromKey(bulk));
        } else if (c == '+') {
            args.push_back(CompactObj::fromKey(read_line()));
        } else if (c == ':') {
            args.push_back(CompactObj::fromKey(read_line()));
        } else if (c == '-') {
            args.push_back(CompactObj::fromKey(read_line()));
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
            value.obj_value = CompactObj::fromKey(read_line());
            return 0;
        case '-':
            value.type = DataType::Error;
            value.obj_value = CompactObj::fromKey(read_line());
            return 0;
        case ':': {
            value.type = DataType::Integer;
            std::string line = read_line();
            int64_t int_val = 0;
            if (!string2ll(line.data(), line.size(), &int_val)) {
                return -1;
            }
            value.obj_value = CompactObj::fromInt(int_val);
            return 0;
        }
        case '$': {
            std::string len_str = read_line();
            int64_t len = 0;
            if (!string2ll(len_str.data(), len_str.size(), &len)) {
                return -1;
            }
            value.type = DataType::BulkString;
            if (len >= 0) {
                value.obj_value = CompactObj::fromKey(read_bulk_string(len));
            } else {
                value.obj_value = CompactObj();
            }
            return 0;
        }
        default:
            return -1;
    }
}

int RESPParser::parse_command(std::vector<CompactObj>& args) {
    args.clear();

    char c = read_char();
    if (c == 0) {
        return -1;
    }

    if (c == '*') {
        return parse_array(args);
    } else {
        std::string line(1, c);
        line += read_line();
        return parse_inline_command(line, args);
    }
}

std::string RESPParser::make_simple_string(const std::string& s) {
    return "+" + s + "\r\n";
}

std::string RESPParser::make_error(const std::string& msg) {
    return "-ERR " + msg + "\r\n";
}

std::string RESPParser::make_bulk_string(const std::string& s) {
    return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
}

std::string RESPParser::make_null_bulk_string() {
    return "$-1\r\n";
}

std::string RESPParser::make_integer(int64_t value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::string RESPParser::make_array(int64_t count) {
    return "*" + std::to_string(count) + "\r\n";
}
