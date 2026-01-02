#include "server/engine_shard.h"
#include "server/engine_shard_set.h"
#include "protocol/resp_parser.h"
#include "command/command_registry.h"
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <photon/thread/st.h>
#include <photon/thread/thread11.h>
#include <photon/net/socket.h>
#include <photon/photon.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>

__thread EngineShard* EngineShard::tlocal_shard_ = nullptr;

EngineShard::EngineShard(size_t shard_id, uint16_t port, EngineShardSet* shard_set)
    : shard_id_(shard_id), db_(new Database()), port_(port), shard_set_(shard_set), running_(false) {
}

EngineShard::~EngineShard() {
	Stop();
	Join();
}

void EngineShard::Start() {
	running_ = true;
	thread_ = std::thread(&EngineShard::EventLoop, this);
}

void EngineShard::Stop() {
	running_ = false;
}

void EngineShard::Join() {
	if (thread_.joinable()) {
		thread_.join();
	}
}

void EngineShard::EventLoop() {
	photon::init(photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE);
	DEFER(photon::fini());

	tlocal_shard_ = this;

	auto server = photon::net::new_tcp_socket_server();
	if (server == nullptr) {
		LOG_ERROR("Failed to create socket server");
		return;
	}

	int ret = server->setsockopt<int>(SOL_SOCKET, SO_REUSEPORT, 1);
	if (ret < 0) {
		delete server;
		LOG_ERROR("Failed to set SO_REUSEPORT");
		return;
	}

	LOG_INFO("EngineShard ", shard_id_, " SO_REUSEPORT set successfully");

	if (server->bind_v4any(port_) < 0) {
		delete server;
		LOG_ERROR("Failed to bind port");
		return;
	}

	if (server->listen(128) < 0) {
		delete server;
		LOG_ERROR("Failed to listen");
		return;
	}

	LOG_INFO("EngineShard ", shard_id_, " listening on port ", port_, " with SO_REUSEPORT");

	photon::thread_create11([this, server]() {
		while (running_) {
			auto client_sock = server->accept();
			if (client_sock) {
				photon::thread_create11([this, client_sock]() {
					HandleConnection(client_sock);
					delete client_sock;
				});
			} else {
				photon::thread_yield();
			}
		}
	});

	photon::thread_create11([this]() {
		uint64_t buf;
		while (running_) {
			ssize_t ret = read(task_queue_.event_fd(), &buf, sizeof(buf));
			if (ret > 0) {
				task_queue_.ProcessTasks();
			}
		}
	});

	while (running_) {
		photon::thread_sleep(1);
	}

	delete server;
}

int EngineShard::HandleConnection(photon::net::ISocketStream* stream) {
	LOG_INFO("EngineShard ", shard_id_, ": New client connected");

	RESPParser parser(stream);

	while (running_) {
		std::vector<CompactObj> args;
		int ret = parser.parse_command(args);

		if (ret < 0) {
			LOG_INFO("EngineShard ", shard_id_, ": Client disconnected");
			return 0;
		}

		if (!args.empty()) {
			std::string response = ProcessCommand(args);

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

std::string EngineShard::ProcessCommand(const std::vector<CompactObj>& args) {
	CommandContext ctx(this, shard_set_, shard_set_ ? shard_set_->size() : 1, db_->CurrentDB());
	return CommandRegistry::instance().execute(args, &ctx);
}
