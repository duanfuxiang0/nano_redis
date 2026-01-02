#include "server/server.h"
#include "protocol/resp_parser.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <photon/thread/st.h>
#include <netinet/tcp.h>

RedisServer::RedisServer()
    : server_(photon::net::new_tcp_socket_server()) {
	StringFamily::Register(&CommandRegistry::instance());
	HashFamily::Register(&CommandRegistry::instance());
	SetFamily::Register(&CommandRegistry::instance());
	ListFamily::Register(&CommandRegistry::instance());
}

RedisServer::~RedisServer() {
	term();
}

std::string RedisServer::process_command(const std::vector<CompactObj>& args) {
	CommandContext ctx(nullptr, nullptr, 1, store_.CurrentDB());
	return CommandRegistry::instance().execute(args, &ctx);
}

int RedisServer::handle_client(photon::net::ISocketStream* stream) {
	LOG_INFO("New client connected");

	RESPParser parser(stream);

	while (true) {
		std::vector<CompactObj> args;
		int ret = parser.parse_command(args);

		if (ret < 0) {
			LOG_INFO("Client disconnected");
			return 0;
		}

		if (!args.empty()) {
			LOG_INFO("Received command: `", args[0].toString());
			std::string response = process_command(args);

			std::string cmd_str = args[0].toString();
			if (cmd_str == "QUIT") {
				stream->send(response.data(), response.size());
				st_usleep(10000);
				return 0;
			}

			ssize_t written = stream->send(response.data(), response.size());
			if (written < 0) {
				LOG_ERRNO_RETURN(0, -1, "Failed to write response");
			}
		}
	}

	return 0;
}

int RedisServer::run(int port) {
	if (server_->bind_v4any(port) < 0) {
		LOG_ERRNO_RETURN(0, -1, "Failed to bind port `", port);
	}
	if (server_->listen() < 0) {
		LOG_ERRNO_RETURN(0, -1, "Failed to listen");
	}
	
	LOG_INFO("Started Redis server at `", server_->getsockname());
	
	server_->set_handler({this, &RedisServer::handle_client});
	
	return server_->start_loop(true);
}

void RedisServer::term() {
	server_->terminate();
}
