#pragma once

#include <photon/net/socket.h>
#include <photon/common/stream.h>
#include <memory>
#include <string>
#include <vector>

#include "core/database.h"
#include "core/command_context.h"
#include "core/nano_obj.h"
#include "command/string_family.h"
#include "command/hash_family.h"
#include "command/set_family.h"
#include "command/list_family.h"

class RedisServer {
public:
	RedisServer();
	~RedisServer();

	int Run(int port);
	void Term();

private:
	std::unique_ptr<photon::net::ISocketServer> server;
	Database store;
	bool command_families_registered = false;

	int HandleClient(photon::net::ISocketStream* stream);
	std::string ProcessCommand(const std::vector<NanoObj>& args);
};
