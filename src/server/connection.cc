#include "server/connection.h"
#include <photon/common/alog.h>

Connection::Connection(photon::net::ISocketStream* socket_value) : socket(socket_value) {
}

void Connection::Close() {
	if (socket) {
		socket->close();
	}
}

void Connection::SendError(const std::string& msg) {
	SendRaw("-" + msg + "\r\n");
}

void Connection::SendSimpleString(const std::string& str) {
	SendRaw("+" + str + "\r\n");
}

void Connection::SendBulkString(const std::string& str) {
	SendRaw("$" + std::to_string(str.size()) + "\r\n" + str + "\r\n");
}

void Connection::SendNullBulkString() {
	SendRaw("$-1\r\n");
}

void Connection::SendInteger(int64_t value) {
	SendRaw(":" + std::to_string(value) + "\r\n");
}

void Connection::SendArray(const std::vector<std::string>& values) {
	SendRaw("*" + std::to_string(values.size()) + "\r\n");
	for (const auto& value : values) {
		if (value.empty()) {
			SendNullBulkString();
		} else {
			SendBulkString(value);
		}
	}
}

void Connection::SendRaw(const std::string& data) {
	if (!socket) {
		LOG_ERRNO_RETURN(0, , "Socket is null");
	}
	ssize_t ret = socket->write(data.data(), data.size());
	if (ret < 0) {
		LOG_ERRNO_RETURN(0, , "Failed to write to socket");
	}
}
