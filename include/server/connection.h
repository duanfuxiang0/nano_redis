#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <photon/net/socket.h>

#include "core/nano_obj.h"
#include "protocol/resp_parser.h"

class Connection {
public:
	explicit Connection(photon::net::ISocketStream* socket);
	~Connection() = default;

	void Close();
	int ParseCommand(std::vector<NanoObj>& args);
	RESPParser::TryParseResult TryParseCommandNoRead(std::vector<NanoObj>& args);

	void SendError(const std::string& msg);
	void SendSimpleString(const std::string& str);
	void SendBulkString(const std::string& str);
	void SendNullBulkString();
	void SendInteger(int64_t value);
	void SendArray(const std::vector<std::string>& values);
	bool SendResponse(std::string_view response);
	void AppendResponse(std::string_view response);
	bool Flush();
	bool HasBufferedInput() const {
		return parser.HasBufferedData();
	}
	size_t PendingResponseBytes() const {
		return write_buffer.size();
	}

	bool SetDBIndex(size_t index);
	size_t GetDBIndex() const {
		return db_index;
	}
	uint64_t GetClientId() const {
		return client_id;
	}
	int64_t GetConnectedAtMs() const {
		return connected_at_ms;
	}
	int64_t GetLastActiveAtMs() const {
		return last_active_at_ms;
	}
	void SetLastCommand(std::string_view command);
	const std::string& GetLastCommand() const {
		return last_command;
	}
	void SetClientName(std::string name) {
		client_name = std::move(name);
	}
	const std::string& GetClientName() const {
		return client_name;
	}
	void RequestClose() {
		close_requested.store(true, std::memory_order_relaxed);
	}
	bool IsCloseRequested() const {
		return close_requested.load(std::memory_order_relaxed);
	}

private:
	photon::net::ISocketStream* socket;
	RESPParser parser;
	uint64_t client_id = 0;
	int64_t connected_at_ms = 0;
	int64_t last_active_at_ms = 0;
	size_t db_index = 0;
	std::string client_name;
	std::string last_command = "unknown";
	std::string write_buffer;
	std::atomic<bool> close_requested {false};

	bool SendRaw(std::string_view data);
};
