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

/**
 * @brief MPMC task-queue with multiple consumer fibers.
 *
 * Inspired by Dragonfly's TaskQueue design:
 * - Lock-free MPMC bounded queue (based on Dmitry Vyukov's algorithm)
 * - Multiple consumer fibers for parallel task execution
 * - Photon semaphore for efficient fiber-friendly notification
 *
 * Design:
 *   Multiple producers (IO threads) -> MPMC Queue -> Multiple consumer fibers
 *
 * Usage:
 *   TaskQueue queue(4096, 1);  // queue_size, num_consumer_fibers
 *   queue.Start("worker");      // Start consumer fibers
 *   queue.Add([]{ ... });       // Submit tasks
 *   queue.Await([]{ return 42; }); // Submit and wait for result
 *   queue.Shutdown();           // Stop consumers
 */
class TaskQueue {
public:
	/**
	 * @brief Construct a TaskQueue
	 * @param queue_size Capacity of the queue (must be power of 2)
	 * @param num_consumers Number of consumer fibers (default: 1)
	 */
	explicit TaskQueue(size_t queue_size = 4096, size_t num_consumers = 1);
	~TaskQueue();

	// Non-copyable
	TaskQueue(const TaskQueue&) = delete;
	TaskQueue& operator=(const TaskQueue&) = delete;

public:
	/**
	 * @brief Try to add a task to the queue (non-blocking)
	 * @return true if task was added, false if queue is full
	 */
	template <typename F>
	bool TryAdd(F&& func);

	/**
	 * @brief Add a task to the queue (may spin if full)
	 * @return true if task was added
	 */
	template <typename F>
	bool Add(F&& func);

	/**
	 * @brief Submit a task and wait for its completion
	 * The calling fiber will suspend (not block the thread) while waiting.
	 * @return The result of func()
	 */
	template <typename F>
	auto Await(F&& func) -> decltype(func());

	/**
	 * @brief Start consumer fibers
	 * Must be called from the owning vCPU thread.
	 * @param base_name Base name for consumer fibers
	 */
	void Start(const std::string& base_name = "tq");

	/**
	 * @brief Shutdown the queue and wait for consumer fibers to finish
	 */
	void Shutdown();

	/**
	 * @brief Check if queue is empty
	 */
	bool Empty() const;

	/**
	 * @brief Process tasks (for external use, deprecated - use Start() instead)
	 */
	void ProcessTasks();

	/**
	 * @brief Get event fd for external polling (deprecated)
	 */
	int event_fd() const { return -1; }

private:
	static size_t RoundUpPowerOfTwo(size_t x);

	using CbFunc = std::function<void()>;

	// MPMC bounded queue cell
	struct Cell {
		std::atomic<size_t> sequence;
		alignas(CbFunc) char storage[sizeof(CbFunc)];
	};

	bool TryEnqueue(CbFunc&& func);
	bool TryDequeue(CbFunc& func);
	void Run();  // Consumer fiber main loop

private:
	static constexpr size_t kCacheLineSize = 64;

	// Queue storage
	std::unique_ptr<Cell[]> buffer_;
	size_t capacity_;
	size_t buffer_mask_;

	// Separate cache lines for producer and consumer indices
	alignas(kCacheLineSize) std::atomic<size_t> enqueue_pos_{0};
	alignas(kCacheLineSize) std::atomic<size_t> dequeue_pos_{0};

	// Photon semaphore for consumer notification
	photon::semaphore pull_sem_;
	// Number of consumer fibers currently waiting for work.
	// Used to avoid excessive cross-thread semaphore signaling under load.
	std::atomic<uint64_t> idler_{0};

	// Consumer fibers
	size_t num_consumers_;
	std::vector<photon::join_handle*> consumer_fibers_;
	std::atomic<bool> is_closed_{false};
};

// ============================================================================
// Template implementations
// ============================================================================

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
	static constexpr uint64_t kSleepUsec = 1000;  // 1ms
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
