#pragma once

#include <photon/net/socket.h>
#include <photon/common/stream.h>
#include <memory>
#include <string>
#include <vector>

#include "core/database.h"
#include "core/command_context.h"
#include "core/compact_obj.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"

class RedisServer {
public:
	RedisServer();
	~RedisServer();

	int run(int port);
	void term();

private:
	std::unique_ptr<photon::net::ISocketServer> server_;
	Database store_;

	int handle_client(photon::net::ISocketStream* stream);
	std::string process_command(const std::vector<CompactObj>& args);
};
