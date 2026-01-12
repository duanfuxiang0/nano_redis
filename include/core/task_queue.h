#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <photon/thread/thread.h>
#include <photon/thread/thread11.h>

// 获取当前硬件的缓存行大小，如果不支持则默认 64
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
// 典型的 x86_64 缓存行大小
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

// MPMC 任务队列，支持多个消费协程
class TaskQueue {
public:
	explicit TaskQueue(size_t queue_size = 4096, size_t num_consumers = 1);
	~TaskQueue();

	TaskQueue(const TaskQueue&) = delete;
	TaskQueue& operator=(const TaskQueue&) = delete;

public:
	// 非阻塞添加任务
	template <typename F>
	bool TryAdd(F&& func);

	// 添加任务，队满时等待
	template <typename F>
	bool Add(F&& func);

	// 提交任务并等待结果
	template <typename F>
	auto Await(F&& func) -> decltype(func());

	// 启动消费协程
	void Start(const std::string& base_name = "tq");

	// 关闭队列
	void Shutdown();

	bool Empty() const;
	void ProcessTasks();
	int event_fd() const {
		return -1;
	}

private:
	static size_t RoundUpPowerOfTwo(size_t x);

	using CbFunc = std::function<void()>;

	struct Cell {
		std::atomic<size_t> sequence;
		alignas(CbFunc) char storage[sizeof(CbFunc)];
	};

	bool TryEnqueue(CbFunc&& func);
	bool TryDequeue(CbFunc& func);
	void Run();

private:
	std::unique_ptr<Cell[]> buffer_;
	size_t capacity_;
	size_t buffer_mask_;

	alignas(hardware_destructive_interference_size) std::atomic<size_t> enqueue_pos_ {0};
	alignas(hardware_destructive_interference_size) std::atomic<size_t> dequeue_pos_ {0};

	photon::semaphore pull_sem_;
	std::atomic<uint64_t> idler_ {0};

	size_t num_consumers_;
	std::vector<photon::join_handle*> consumer_fibers_;
	std::atomic<bool> is_closed_ {false};
};

template <typename F>
bool TaskQueue::TryAdd(F&& func) {
	bool enqueued = TryEnqueue(CbFunc(std::forward<F>(func)));
	if (enqueued) {
		// OPTIMIZED: Always signal consumer to avoid missed notifications
		// The old logic (only signal when idler_ != 0) caused problems
		// under high load: consumers keep working (idler_ = 0),
		// new tasks enter queue but consumers are never signaled.
		pull_sem_.signal(1);
		return true;
	}
	return false;
}

template <typename F>
bool TaskQueue::Add(F&& func) {
	// Fast path
	if (TryAdd(std::forward<F>(func))) {
		return true;
	}

	// Slow path - sleep instead of spin with yield
	// photon::thread_yield() only switches fibers within the current vCPU,
	// not releasing the OS thread. This causes deadlock under high load:
	// - Many connection fibers spin here, occupying the OS thread
	// - Consumer fibers never get a chance to execute
	// - Use thread_usleep() to truly release the OS thread
	static constexpr uint64_t kSleepUsec = 1000; // 1ms
	while (!is_closed_.load(std::memory_order_relaxed)) {
		photon::thread_usleep(kSleepUsec);
		if (TryAdd(std::forward<F>(func))) {
			return true;
		}
	}
	return false;
}

template <typename F>
auto TaskQueue::Await(F&& func) -> decltype(func()) {
	using RetType = decltype(func());

	if constexpr (std::is_void_v<RetType>) {
		photon::semaphore done_sem(0);
		std::exception_ptr exception;

		Add([&func, &done_sem, &exception]() {
			try {
				func();
			} catch (...) {
				exception = std::current_exception();
			}
			done_sem.signal(1);
		});

		done_sem.wait(1);

		if (exception) {
			std::rethrow_exception(exception);
		}
	} else {
		photon::semaphore done_sem(0);
		std::optional<RetType> result;
		std::exception_ptr exception;

		Add([&func, &done_sem, &result, &exception]() {
			try {
				result = func();
			} catch (...) {
				exception = std::current_exception();
			}
			done_sem.signal(1);
		});

		done_sem.wait(1);

		if (exception) {
			std::rethrow_exception(exception);
		}

		return std::move(*result);
	}
}
