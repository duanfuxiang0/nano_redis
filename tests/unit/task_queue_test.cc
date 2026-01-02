#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "core/task_queue.h"

class TaskQueueTest : public ::testing::Test {
protected:
	void SetUp() override {
		queue_ = new TaskQueue(4096);
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
	std::atomic<bool> consumer_done(false);

	std::thread producer([&]() {
		for (int i = 0; i < kNumTasks; ++i) {
			while (!queue_->TryAdd([&completed_count]() {
				completed_count++;
			})) {
				std::this_thread::yield();
			}
		}
	});

	std::thread consumer([&]() {
		while (completed_count < kNumTasks) {
			queue_->ProcessTasks();
		}
		consumer_done = true;
	});

	producer.join();
	consumer.join();

	EXPECT_EQ(completed_count, kNumTasks);
	EXPECT_TRUE(consumer_done);
}

TEST_F(TaskQueueTest, MultipleProducers) {
	const int kNumProducers = 2;
	const int kTasksPerProducer = 50;
	const int kTotalTasks = kNumProducers * kTasksPerProducer;
	std::atomic<int> completed_count(0);
	std::atomic<bool> consumer_done(false);
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
		});
	}

	std::thread consumer([&]() {
		while (completed_count < kTotalTasks) {
			queue_->ProcessTasks();
		}
		consumer_done = true;
	});

	for (auto& t : producers) {
		t.join();
	}
	consumer.join();

	EXPECT_EQ(completed_count, kTotalTasks);
	EXPECT_TRUE(consumer_done);
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
	TaskQueue small_queue(kCapacity);

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
