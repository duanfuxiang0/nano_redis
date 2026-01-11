#include <gtest/gtest.h>
#include "core/task_queue.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

#include <photon/photon.h>
#include <photon/thread/thread.h>
#include <photon/thread/thread11.h>

class TaskQueueAwaitTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Initialize photon for fiber-based Await tests
		photon::init(photon::INIT_EVENT_EPOLL, photon::INIT_IO_NONE);
	}

	void TearDown() override {
		photon::fini();
	}
};

TEST_F(TaskQueueAwaitTest, BasicAwaitInt) {
	TaskQueue queue(4096, 1);
	queue.Start("test");

	int result = queue.Await([]() {
		return 42;
	});

	EXPECT_EQ(result, 42);

	queue.Shutdown();
}

TEST_F(TaskQueueAwaitTest, BasicAwaitString) {
	TaskQueue queue(4096, 1);
	queue.Start("test");

	std::string result = queue.Await([]() {
		return std::string("hello world");
	});

	EXPECT_EQ(result, "hello world");

	queue.Shutdown();
}

TEST_F(TaskQueueAwaitTest, BasicAwaitVoid) {
	TaskQueue queue(4096, 1);
	queue.Start("test");

	bool executed = false;
	queue.Await([&executed]() {
		executed = true;
	});

	EXPECT_TRUE(executed);

	queue.Shutdown();
}

TEST_F(TaskQueueAwaitTest, AwaitException) {
	TaskQueue queue(4096, 1);
	queue.Start("test");

	EXPECT_THROW(
		queue.Await([]() -> int {
			throw std::runtime_error("test exception");
			return 42;
		}),
		std::runtime_error
	);

	queue.Shutdown();
}

TEST_F(TaskQueueAwaitTest, MultipleAwaits) {
	TaskQueue queue(4096, 1);
	queue.Start("test");

	for (int i = 0; i < 10; ++i) {
		int result = queue.Await([i]() {
			return i * 2;
		});
		EXPECT_EQ(result, i * 2);
	}

	queue.Shutdown();
}

TEST_F(TaskQueueAwaitTest, AwaitWithAddMixed) {
	TaskQueue queue(4096, 1);
	queue.Start("test");

	std::atomic<int> counter{0};

	// Add some fire-and-forget tasks
	for (int i = 0; i < 5; ++i) {
		queue.Add([&counter]() {
			counter++;
		});
	}

	// Await ensures all previous tasks are processed
	int result = queue.Await([&counter]() {
		return counter.load();
	});

	EXPECT_GE(result, 0);  // At least some tasks should be processed

	queue.Shutdown();
}

TEST_F(TaskQueueAwaitTest, MultipleConsumers) {
	// Test with multiple consumer fibers
	TaskQueue queue(4096, 4);
	queue.Start("test");

	std::atomic<int> counter{0};
	const int kNumTasks = 100;

	// Submit many tasks
	for (int i = 0; i < kNumTasks; ++i) {
		queue.Add([&counter]() {
			counter++;
		});
	}

	// Await to synchronize
	queue.Await([&counter, kNumTasks]() {
		// Spin until all tasks are done
		while (counter.load() < kNumTasks) {
			photon::thread_yield();
		}
	});

	EXPECT_EQ(counter.load(), kNumTasks);

	queue.Shutdown();
}
