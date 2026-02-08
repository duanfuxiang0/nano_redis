#include "server/server.h"
#include "protocol/resp_parser.h"
#include "command/command_registry.h"
#include "command/string_family.h"
#include "core/util.h"
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <photon/common/utility.h>
#include <photon/thread/st.h>
#include <gflags/gflags.h>
#include <netinet/tcp.h>

DECLARE_bool(tcp_nodelay);
DECLARE_bool(use_iouring_tcp_server);

RedisServer::RedisServer() = default;

RedisServer::~RedisServer() {
	Term();
}

std::string RedisServer::ProcessCommand(const std::vector<NanoObj>& args) {
	CommandContext ctx(&store, store.CurrentDB());
	return CommandRegistry::Instance().Execute(args, &ctx);
}

int RedisServer::HandleClient(photon::net::ISocketStream* stream) {
	// LOG_INFO("New client connected");
	if (stream == nullptr) {
		return -1;
	}
	DEFER(stream->close());

	const int nodelay = FLAGS_tcp_nodelay ? 1 : 0;
	(void)stream->setsockopt(IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

	RESPParser parser(stream);

	std::vector<NanoObj> args;
	while (true) {
		args.clear();
		int ret = parser.ParseCommand(args);
		if (ret < 0) {
			return 0;
		}
		if (args.empty()) {
			continue;
		}

		// Handle QUIT early (before command execution)
		const std::string_view cmd_sv = args[0].GetStringView();
		if (!cmd_sv.empty() && EqualsIgnoreCase(cmd_sv, "QUIT")) {
			const std::string& ok = RESPParser::OkResponse();
			stream->send(ok.data(), ok.size());
			st_usleep(10000);
			return 0;
		}

		std::string response = ProcessCommand(args);
		ssize_t written = stream->send(response.data(), response.size());
		if (written < 0) {
			LOG_ERRNO_RETURN(0, -1, "Failed to write response");
		}
	}
}

int RedisServer::Run(int port) {
	if (!command_families_registered) {
		StringFamily::Register(&CommandRegistry::Instance());
		HashFamily::Register(&CommandRegistry::Instance());
		SetFamily::Register(&CommandRegistry::Instance());
		ListFamily::Register(&CommandRegistry::Instance());
		command_families_registered = true;
	}

	if (!server) {
		server.reset(FLAGS_use_iouring_tcp_server ? photon::net::new_iouring_tcp_server()
		                                          : photon::net::new_tcp_socket_server());
		if (server == nullptr && FLAGS_use_iouring_tcp_server) {
			LOG_WARN("Failed to create io_uring TCP server, falling back to syscall TCP server");
			server.reset(photon::net::new_tcp_socket_server());
		}
		if (server == nullptr) {
			LOG_ERROR_RETURN(0, -1, "Failed to create TCP server");
		}
	}

	if (server->bind_v4any(port) < 0) {
		LOG_ERRNO_RETURN(0, -1, "Failed to bind port `", port);
	}
	if (server->listen() < 0) {
		LOG_ERRNO_RETURN(0, -1, "Failed to listen");
	}

	LOG_INFO("Started Redis server at `", server->getsockname());

	server->set_handler({this, &RedisServer::HandleClient});

	return server->start_loop(true);
}

void RedisServer::Term() {
	if (!server) {
		return;
	}
	server->terminate();
}
