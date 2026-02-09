#include "server/connection.h"
#include "core/database.h"
#include <photon/common/alog.h>
#include <atomic>
#include <chrono>

namespace {

int64_t CurrentTimeMs() {
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::milliseconds;
	return std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
}

std::atomic<uint64_t> g_next_client_id {1};

} // namespace

Connection::Connection(photon::net::ISocketStream* socket_value) : socket(socket_value), parser(socket_value) {
	client_id = g_next_client_id.fetch_add(1, std::memory_order_relaxed);
	const int64_t now_ms = CurrentTimeMs();
	connected_at_ms = now_ms;
	last_active_at_ms = now_ms;
}

void Connection::Close() {
	if (socket) {
		socket->close();
	}
}

int Connection::ParseCommand(std::vector<NanoObj>& args) {
	return parser.ParseCommand(args);
}

RESPParser::TryParseResult Connection::TryParseCommandNoRead(std::vector<NanoObj>& args) {
	return parser.TryParseCommandNoRead(args);
}

void Connection::SendError(const std::string& msg) {
	(void)SendRaw("-" + msg + "\r\n");
}

void Connection::SendSimpleString(const std::string& str) {
	(void)SendRaw("+" + str + "\r\n");
}

void Connection::SendBulkString(const std::string& str) {
	(void)SendRaw("$" + std::to_string(str.size()) + "\r\n" + str + "\r\n");
}

void Connection::SendNullBulkString() {
	(void)SendRaw("$-1\r\n");
}

void Connection::SendInteger(int64_t value) {
	(void)SendRaw(":" + std::to_string(value) + "\r\n");
}

void Connection::SendArray(const std::vector<std::string>& values) {
	(void)SendRaw("*" + std::to_string(values.size()) + "\r\n");
	for (const auto& value : values) {
		if (value.empty()) {
			SendNullBulkString();
		} else {
			SendBulkString(value);
		}
	}
}

bool Connection::SendResponse(std::string_view response) {
	AppendResponse(response);
	return Flush();
}

void Connection::AppendResponse(std::string_view response) {
	write_buffer.append(response.data(), response.size());
}

bool Connection::Flush() {
	if (write_buffer.empty()) {
		return true;
	}
	if (!SendRaw(write_buffer)) {
		return false;
	}
	write_buffer.clear();
	return true;
}

bool Connection::SetDBIndex(size_t index) {
	if (index >= Database::kNumDBs) {
		return false;
	}
	db_index = index;
	return true;
}

void Connection::SetLastCommand(std::string_view command) {
	last_active_at_ms = CurrentTimeMs();
	last_command.assign(command.data(), command.size());
}

bool Connection::SendRaw(std::string_view data) {
	if (!socket) {
		LOG_ERROR("Socket is null");
		return false;
	}

	size_t total_sent = 0;
	while (total_sent < data.size()) {
		ssize_t ret = socket->send(data.data() + total_sent, data.size() - total_sent);
		if (ret <= 0) {
			LOG_ERROR("Failed to write to socket");
			return false;
		}
		total_sent += static_cast<size_t>(ret);
	}

	return true;
}
