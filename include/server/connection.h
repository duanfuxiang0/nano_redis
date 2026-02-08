#pragma once

#include <string>
#include <vector>
#include <photon/net/socket.h>

class Connection {
public:
	explicit Connection(photon::net::ISocketStream* socket);
	~Connection() = default;

	void Close();

	void SendError(const std::string& msg);
	void SendSimpleString(const std::string& str);
	void SendBulkString(const std::string& str);
	void SendNullBulkString();
	void SendInteger(int64_t value);
	void SendArray(const std::vector<std::string>& values);

private:
	photon::net::ISocketStream* socket;

	void SendRaw(const std::string& data);
};
