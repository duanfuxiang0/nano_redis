#include "server/server.h"
#include "protocol/resp_parser.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <photon/common/utility.h>
#include <photon/thread/st.h>
#include <gflags/gflags.h>
#include <netinet/tcp.h>

DECLARE_bool(tcp_nodelay);
DECLARE_bool(use_iouring_tcp_server);

RedisServer::RedisServer()
    : server_(FLAGS_use_iouring_tcp_server ? photon::net::new_iouring_tcp_server()
                                           : photon::net::new_tcp_socket_server()) {
	if (server_ == nullptr && FLAGS_use_iouring_tcp_server) {
		LOG_WARN("Failed to create io_uring TCP server, falling back to syscall TCP server");
		server_.reset(photon::net::new_tcp_socket_server());
	}
	StringFamily::Register(&CommandRegistry::instance());
	HashFamily::Register(&CommandRegistry::instance());
	SetFamily::Register(&CommandRegistry::instance());
	ListFamily::Register(&CommandRegistry::instance());
}

RedisServer::~RedisServer() {
	term();
}

std::string RedisServer::process_command(const std::vector<NanoObj>& args) {
	CommandContext ctx(&store_, store_.CurrentDB());
	return CommandRegistry::instance().execute(args, &ctx);
}

int RedisServer::handle_client(photon::net::ISocketStream* stream) {
	// LOG_INFO("New client connected");
	if (stream == nullptr) {
		return -1;
	}
	DEFER(stream->close());

	const int nodelay = FLAGS_tcp_nodelay ? 1 : 0;
	(void)stream->setsockopt(IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

	RESPParser parser(stream);

	while (true) {
		std::vector<NanoObj> args;
		int ret = parser.parse_command(args);

		if (ret < 0) {
			// LOG_INFO("Client disconnected");
			return 0;
		}

		if (!args.empty()) {
			// LOG_INFO("Received command: `", args[0].toString());
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
