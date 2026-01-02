#include <gflags/gflags.h>
#include <photon/common/utility.h>
#include <photon/io/signal.h>
#include <photon/photon.h>
#include <photon/common/alog.h>

#include "server/server.h"
#include "server/sharded_server.h"

DEFINE_int32(port, 0, "Server listen port, 0(default) for random port");
DEFINE_int32(num_shards, 1, "Number of shards (default 1 for single-threaded mode)");

std::unique_ptr<RedisServer> redisserver;
std::unique_ptr<ShardedServer> sharded_server;

void handle_null(int) {
}
void handle_term(int) {
	redisserver.reset();
	sharded_server.reset();
}

int main(int argc, char** argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	set_log_output_level(ALOG_INFO);

	if (FLAGS_num_shards > 1) {
		LOG_INFO("Starting in multi-threaded mode with ", FLAGS_num_shards, " shards");
		sharded_server.reset(new ShardedServer(FLAGS_num_shards, FLAGS_port));
		sharded_server->Run();
	} else {
		photon::PhotonOptions options;
		options.iouring_sq_thread_idle_ms = 1000;

		uint64_t event_engine = photon::INIT_EVENT_SIGNAL;
		uint64_t io_engine = photon::INIT_IO_NONE;
		int ret = photon::init(event_engine, io_engine, options);
		if (ret < 0) {
			LOG_ERROR_RETURN(0, -1, "Failed to initialize photon with io_uring");
		}
		DEFER(photon::fini());

		photon::sync_signal(SIGPIPE, &handle_null);
		photon::sync_signal(SIGTERM, &handle_term);
		photon::sync_signal(SIGINT, &handle_term);

		LOG_INFO("Starting in single-threaded mode");
		redisserver.reset(new RedisServer());
		redisserver->run(FLAGS_port);
	}

	return 0;
}
