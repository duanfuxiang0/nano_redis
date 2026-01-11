#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "core/task_queue.h"

class TaskQueueTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Create queue without starting consumer fibers for basic tests
		queue_ = new TaskQueue(4096, 0);
	}

	void TearDown() override {
		delete queue_;
	}

	TaskQueue* queue_ = nullptr;
};

TEST_F(TaskQueueTest, BasicEnqueueDequeue) {
	bool executed = false;
	ASSERT_TRUE(queue_->TryAdd([&executed]() { executed = true; }));

	EXPECT_FALSE(executed);

	queue_->ProcessTasks();

	EXPECT_TRUE(executed);
}

TEST_F(TaskQueueTest, MultipleTasks) {
	int counter = 0;
	const int kNumTasks = 100;

	for (int i = 0; i < kNumTasks; ++i) {
		ASSERT_TRUE(queue_->TryAdd([&counter]() { counter++; }));
	}

	EXPECT_EQ(counter, 0);

	queue_->ProcessTasks();

	EXPECT_EQ(counter, kNumTasks);
}

TEST_F(TaskQueueTest, ProducerConsumer) {
	const int kNumTasks = 1000;
	std::atomic<int> completed_count(0);
	std::atomic<bool> producer_done(false);

	std::thread producer([&]() {
		for (int i = 0; i < kNumTasks; ++i) {
			while (!queue_->TryAdd([&completed_count]() {
				completed_count++;
			})) {
				std::this_thread::yield();
			}
		}
		producer_done = true;
	});

	std::thread consumer([&]() {
		while (!producer_done || completed_count < kNumTasks) {
			queue_->ProcessTasks();
			std::this_thread::yield();
		}
	});

	producer.join();
	consumer.join();

	EXPECT_EQ(completed_count, kNumTasks);
}

TEST_F(TaskQueueTest, MultipleProducers) {
	const int kNumProducers = 2;
	const int kTasksPerProducer = 50;
	const int kTotalTasks = kNumProducers * kTasksPerProducer;
	std::atomic<int> completed_count(0);
	std::atomic<int> producers_done(0);
	std::vector<std::thread> producers;

	for (int p = 0; p < kNumProducers; ++p) {
		producers.emplace_back([&, p]() {
			int local_count = 0;
			for (int i = 0; i < kTasksPerProducer; ++i) {
				while (!queue_->TryAdd([&completed_count]() {
					completed_count++;
				})) {
					std::this_thread::yield();
				}
				local_count++;
			}
			producers_done++;
		});
	}

	std::thread consumer([&]() {
		while (producers_done < kNumProducers || completed_count < kTotalTasks) {
			queue_->ProcessTasks();
			std::this_thread::yield();
		}
	});

	for (auto& t : producers) {
		t.join();
	}
	consumer.join();

	EXPECT_EQ(completed_count, kTotalTasks);
}

TEST_F(TaskQueueTest, EmptyQueue) {
	EXPECT_TRUE(queue_->Empty());

	queue_->ProcessTasks();
	EXPECT_TRUE(queue_->Empty());
}

TEST_F(TaskQueueTest, NonEmptyQueue) {
	queue_->TryAdd([]() {});

	EXPECT_FALSE(queue_->Empty());
}

TEST_F(TaskQueueTest, QueueCapacity) {
	const size_t kCapacity = 4096;
	TaskQueue small_queue(kCapacity, 0);

	size_t count = 0;
	for (; count < kCapacity + 100; ++count) {
		if (!small_queue.TryAdd([]() {})) {
			break;
		}
	}

	EXPECT_GE(count, kCapacity - 1);
	EXPECT_LE(count, kCapacity);
}

TEST_F(TaskQueueTest, TaskWithCapture) {
	std::string expected = "hello";
	std::string actual;

	queue_->TryAdd([&expected, &actual]() {
		actual = expected;
	});

	queue_->ProcessTasks();

	EXPECT_EQ(actual, expected);
}
