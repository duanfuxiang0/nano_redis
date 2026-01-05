#include <gflags/gflags.h>
#include <photon/common/utility.h>
#include <photon/io/signal.h>
#include <photon/photon.h>
#include <photon/common/alog.h>

#include "server/server.h"
#include "server/sharded_server.h"

DEFINE_int32(port, 9527, "Server listen port");
DEFINE_int32(num_shards, 8, "Number of shards");

std::unique_ptr<RedisServer> redisserver;
std::unique_ptr<ShardedServer> sharded_server;

void handle_null(int) {
}
void handle_term(int signum) {
	LOG_INFO("Received signal `, initiating shutdown...", signum);
	if (redisserver) {
		redisserver->term();
	}
	if (sharded_server) {
		sharded_server->Stop();
	}
}

int main(int argc, char** argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	set_log_output_level(ALOG_INFO);

	// Try to initialize photon with io_uring first (best performance)
	int ret = photon::init(
		photon::INIT_EVENT_IOURING | photon::INIT_EVENT_SIGNAL,
		photon::INIT_IO_NONE
	);
	if (ret < 0) {
		LOG_WARN("Failed to initialize photon with io_uring, falling back to epoll");
		// Fallback to epoll if io_uring is not available
		ret = photon::init(
			photon::INIT_EVENT_EPOLL | photon::INIT_EVENT_SIGNAL,
			photon::INIT_IO_NONE
		);
		if (ret < 0) {
			LOG_ERROR_RETURN(0, -1, "Failed to initialize photon with epoll");
		}
		LOG_INFO("Main thread initialized with epoll");
	} else {
		LOG_INFO("Main thread initialized with io_uring");
	}
	DEFER(photon::fini());

	photon::sync_signal(SIGPIPE, &handle_null);
	photon::sync_signal(SIGTERM, &handle_term);
	photon::sync_signal(SIGINT, &handle_term);

	if (FLAGS_num_shards > 1) {
		LOG_INFO("Starting in multi-threaded mode with ", FLAGS_num_shards, " shards");
		sharded_server.reset(new ShardedServer(FLAGS_num_shards, FLAGS_port));
		sharded_server->Run();
	} else {
		LOG_INFO("Starting in single-threaded mode");
		redisserver.reset(new RedisServer());
		redisserver->run(FLAGS_port);
	}

	return 0;
}
