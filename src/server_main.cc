#include <gflags/gflags.h>
#include <photon/common/utility.h>
#include <photon/io/signal.h>
#include <photon/photon.h>
#include <photon/common/alog.h>
#include <netinet/tcp.h>

#include "server/server.h"
#include "server/sharded_server.h"

DEFINE_int32(port, 9527, "Server listen port");
DEFINE_int32(num_shards, 8, "Number of shards");
DEFINE_bool(tcp_nodelay, true,
            "Enable TCP_NODELAY (lower latency, usually lower throughput in small-reply benchmarks)");
DEFINE_bool(use_iouring_tcp_server, true,
            "Use Photon io_uring TCP server implementation (fallback to syscall-based server if unavailable)");

DEFINE_uint64(photon_handler_stack_kb, 256,
              "Photon per-connection handler fiber stack size in KB (default Photon is 8192KB)");

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" uint64_t nano_redis_photon_handler_stack_size() {
	return FLAGS_photon_handler_stack_kb * 1024ULL;
}

namespace {

RedisServer redis_server;
std::unique_ptr<ShardedServer> sharded_server;

void HandleNull(int) {
}

void HandleTerm(int signum) {
	LOG_INFO("Received signal `, initiating shutdown...", signum);
	redis_server.Term();
	if (sharded_server) {
		sharded_server->Term();
	}
}

} // namespace

int main(int argc, char** argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	set_log_output_level(ALOG_INFO);

	photon::PhotonOptions photon_options;
	photon_options.use_pooled_stack_allocator = true;

	// Initialize photon with io_uring first
	int ret =
	    photon::init(photon::INIT_EVENT_IOURING | photon::INIT_EVENT_SIGNAL, photon::INIT_IO_NONE, photon_options);
	if (ret < 0) {
		LOG_WARN("Failed to initialize photon with io_uring, falling back to epoll");
		// Fallback to epoll if io_uring is not available
		ret = photon::init(photon::INIT_EVENT_EPOLL | photon::INIT_EVENT_SIGNAL, photon::INIT_IO_NONE, photon_options);
		if (ret < 0) {
			LOG_ERROR_RETURN(0, -1, "Failed to initialize photon with epoll");
		}
		LOG_INFO("Main thread initialized with epoll");
	} else {
		LOG_INFO("Main thread initialized with io_uring");
	}
	DEFER(photon::fini());

	photon::sync_signal(SIGPIPE, &HandleNull);
	photon::sync_signal(SIGTERM, &HandleTerm);
	photon::sync_signal(SIGINT, &HandleTerm);

	if (FLAGS_num_shards > 1) {
		LOG_INFO("Starting in multi-threaded mode with ", FLAGS_num_shards, " shards");
		sharded_server = std::make_unique<ShardedServer>(FLAGS_num_shards, FLAGS_port);
		return sharded_server->Run();
	} else {
		LOG_INFO("Starting in single-threaded mode");
		return redis_server.Run(FLAGS_port);
	}

	return 0;
}
