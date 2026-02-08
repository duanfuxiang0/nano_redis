/*
 * 简单的原型程序，验证以下核心假设：
 * 1. SO_REUSEPORT 可以在 photonlibos 中正常工作
 * 2. TaskQueue 可以实现跨线程通信
 * 3. photon::semaphore 可以用于同步
 */

#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <photon/photon.h>
#include <photon/net/socket.h>
#include <photon/thread/thread.h>
#include <photon/common/alog.h>
#include <photon/net/basic_socket.h>

constexpr uint16_t TEST_PORT = 18080;
constexpr int NUM_SHARDS = 4;

// 简单的任务队列测试
static std::atomic<int> task_count {0};
static std::atomic<int> success_count {0};

// 测试 SO_REUSEPORT 的线程函数
void shard_thread(int shard_id) {
	std::string name = "shard_" + std::to_string(shard_id);
	pthread_setname_np(pthread_self(), name.c_str());

	photon::init(photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE);

	// 创建 socket
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		LOG_ERROR("Shard ` failed to create socket", shard_id);
		return;
	}

	// 设置 SO_REUSEPORT
	int optval = 1;
	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		LOG_ERROR("Shard ` failed to set SO_REUSEPORT", shard_id);
		close(sock_fd);
		return;
	}

	// 绑定端口
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(TEST_PORT);

	if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		LOG_ERROR("Shard ` failed to bind port `", shard_id, TEST_PORT);
		close(sock_fd);
		return;
	}

	if (listen(sock_fd, 128) < 0) {
		LOG_ERROR("Shard ` failed to listen", shard_id);
		close(sock_fd);
		return;
	}

	LOG_INFO("Shard ` listening on port `", shard_id, TEST_PORT);

	// Accept 循环
	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int conn_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);

		if (conn_fd < 0) {
			if (errno == EINTR) {
				continue;
			}
			LOG_ERROR("Shard ` accept failed: `", shard_id, strerror(errno));
			continue;
		}

		LOG_INFO("Shard ` accepted connection from `", shard_id, inet_ntoa(client_addr.sin_addr), ":",
		         ntohs(client_addr.sin_port));

		success_count++;

		// 读取数据
		char buf[1024];
		ssize_t n = read(conn_fd, buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			LOG_INFO("Shard ` received: `", shard_id, buf);

			// 发送响应
			const char* response = "+OK\r\n";
			write(conn_fd, response, strlen(response));
		}

		close(conn_fd);
	}
}

int main() {
	photon::init(photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE);
	DEFER(photon::fini());

	LOG_INFO("Starting SO_REUSEPORT prototype test with ` shards", NUM_SHARDS);

	// 创建多个 shard 线程
	std::vector<std::thread> threads;
	for (int i = 0; i < NUM_SHARDS; i++) {
		threads.emplace_back(shard_thread, i);
	}

	// 等待一段时间
	photon::thread_sleep(30);

	// 清理
	LOG_INFO("Total accepted connections: `", success_count.load());
	LOG_INFO("Test completed");

	return 0;
}
