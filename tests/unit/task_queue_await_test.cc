#include <gtest/gtest.h>
#include "core/task_queue.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

TEST(TaskQueueAwaitTest, BasicAwaitInt) {
	TaskQueue queue;

	std::thread processor([&queue]() {
			uint64_t buf;
			while (true) {
				ssize_t ret = read(queue.event_fd(), &buf, sizeof(buf));
				if (ret > 0) {
					queue.ProcessTasks();
					break;
				}
			}
		});

	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	auto task = []() {
		return 42;
	};

	int result = queue.Await(task);

	EXPECT_EQ(result, 42);

	processor.join();
}

TEST(TaskQueueAwaitTest, BasicAwaitString) {
	TaskQueue queue;

	std::thread processor([&]() {
			uint64_t buf;
			while (true) {
				ssize_t ret = read(queue.event_fd(), &buf, sizeof(buf));
				if (ret > 0) {
					queue.ProcessTasks();
					break;
				}
			}
		});

	auto task = []() {
		return std::string("hello world");
	};

	std::string result = queue.Await(task);

	EXPECT_EQ(result, "hello world");

	processor.join();
}

TEST(TaskQueueAwaitTest, BasicAwaitVoid) {
	TaskQueue queue;

	std::thread processor([&]() {
			uint64_t buf;
			while (true) {
				ssize_t ret = read(queue.event_fd(), &buf, sizeof(buf));
				if (ret > 0) {
					queue.ProcessTasks();
					break;
				}
			}
		});

	bool executed = false;
	auto task = [&executed]() {
		executed = true;
	};

	queue.Await(task);

	EXPECT_TRUE(executed);

	processor.join();
}

TEST(TaskQueueAwaitTest, AwaitException) {
	TaskQueue queue;

	std::thread processor([&]() {
			uint64_t buf;
			while (true) {
				ssize_t ret = read(queue.event_fd(), &buf, sizeof(buf));
				if (ret > 0) {
					queue.ProcessTasks();
					break;
				}
			}
		});

	auto task = []() {
		throw std::runtime_error("test exception");
		return 42;
	};

	EXPECT_THROW(queue.Await(task), std::runtime_error);

	processor.join();
}
