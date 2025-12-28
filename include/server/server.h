#pragma once

#include <photon/net/socket.h>
#include <photon/common/stream.h>
#include <memory>
#include <string>
#include <vector>
#include "storage/database.h"
#include "command/string_family.h"

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
    std::string process_command(const std::vector<std::string>& args);
};
