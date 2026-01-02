/*
 * TaskQueue 原型：验证跨线程任务队列
 * 核心假设：
 * 1. photon::semaphore 可以用于跨线程同步
 * 2. 可以实现 MPSC (多生产者单消费者) 队列
 */

#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <iostream>
#include <memory>
#include <cstring>

#include <photon/photon.h>
#include <photon/thread/thread.h>
#include <photon/common/alog.h>

// 简单的 MPSC 队列
class SimpleTaskQueue {
public:
    static constexpr size_t kCapacity = 1024;
    static constexpr size_t kMask = kCapacity - 1;

    struct Task {
        std::function<void()> func;
        bool completed = false;
    };

    SimpleTaskQueue() : buffer_(new Task[kCapacity]) {}

    ~SimpleTaskQueue() = default;

    bool TryAdd(std::function<void()>&& func) {
        uint64_t tail = tail_.load(std::memory_order_relaxed);

        // 简单的回退机制
        for (int i = 0; i < 10; ++i) {
            uint64_t pos = tail & kMask;

            // 检查是否有空间
            if (head_.load(std::memory_order_acquire) == pos) {
                return false;  // 队列满
            }

            buffer_[pos].func = std::move(func);
            buffer_[pos].completed = false;

            uint64_t new_tail = tail + 1;
            if (tail_.compare_exchange_weak(tail, new_tail,
                                          std::memory_order_acq_rel)) {
                return true;  // 成功添加
            }
        }

        return false;
    }

    bool ProcessNext() {
        uint64_t head = head_.load(std::memory_order_relaxed);

        uint64_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) {
            return false;  // 队列空
        }

        uint64_t pos = head & kMask;

        // 执行任务
        if (buffer_[pos].func) {
            buffer_[pos].func();
            buffer_[pos].func = nullptr;
            buffer_[pos].completed = true;
        }

        head_.store(head + 1, std::memory_order_release);
        return true;
    }

private:
    std::unique_ptr<Task[]> buffer_;
    std::atomic<uint64_t> head_{0};
    std::atomic<uint64_t> tail_{0};
};

// 测试：生产者向队列发送任务，消费者执行任务
static std::atomic<int> g_producer_count{0};
static std::atomic<int> g_consumer_count{0};

void producer_thread(SimpleTaskQueue* queue, int producer_id) {
    std::string name = "producer_" + std::to_string(producer_id);
    pthread_setname_np(pthread_self(), name.c_str());

    // 向队列发送任务
    for (int i = 0; i < 100; ++i) {
        auto task = [producer_id, i]() {
            LOG_INFO("Producer `, Task ` executed", producer_id, i);
            g_consumer_count++;
        };

        while (!queue->TryAdd(std::move(task))) {
            photon::thread_yield();  // 队列满，让出 CPU
        }

        g_producer_count++;
        photon::thread_sleep(10);  // 模拟工作间隔
    }

    LOG_INFO("Producer ` finished, sent ` tasks", producer_id, 100);
}

void consumer_thread(SimpleTaskQueue* queue) {
    std::string name = "consumer";
    pthread_setname_np(pthread_self(), name.c_str());

    // 消费队列中的任务
    while (g_consumer_count.load() < 400) {  // 4 producers * 100 tasks
        if (!queue->ProcessNext()) {
            photon::thread_yield();  // 队列空，让出 CPU
        }
    }

    LOG_INFO("Consumer finished, processed ` tasks", g_consumer_count.load());
}

int main() {
    photon::init(photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE);
    DEFER(photon::fini());

    LOG_INFO("Starting TaskQueue prototype test");

    SimpleTaskQueue queue;

    // 创建生产者线程
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back(producer_thread, &queue, i);
    }

    // 等待生产者启动
    photon::thread_sleep(100);

    // 创建消费者线程
    std::thread consumer(consumer_thread, &queue);

    // 等待生产者完成
    for (auto& t : producers) {
        t.join();
    }

    // 等待消费者完成
    consumer.join();

    LOG_INFO("Test completed: producers sent `, consumers processed `",
             g_producer_count.load(), g_consumer_count.load());

    return 0;
}
