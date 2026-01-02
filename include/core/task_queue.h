#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <photon/thread/st.h>
#include <photon/thread/thread.h>
#include <photon/thread/awaiter.h>
#include <photon/common/alog.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <stdexcept>

class TaskQueue {
public:
	explicit TaskQueue(size_t capacity = 4096);
	~TaskQueue();

	template<typename F>
	bool TryAdd(F&& func);

	template<typename F>
	bool Add(F&& func);

	template<typename F>
	auto Await(F&& func) -> decltype(func()) {
		using RetType = decltype(func());

		if constexpr (std::is_void<RetType>::value) {
			std::exception_ptr exception_ptr = nullptr;
			photon::Awaiter<photon::AutoContext> awaiter;

			auto task = [&exception_ptr, &awaiter, &func]() {
				try {
					func();
				} catch (...) {
					exception_ptr = std::current_exception();
				}
				awaiter.resume();
			};

			if (!Add(std::move(task))) {
				throw std::runtime_error("TaskQueue is full");
			}

			awaiter.suspend();

			if (exception_ptr) {
				std::rethrow_exception(exception_ptr);
			}
		} else {
			std::optional<RetType> result_storage;
			std::exception_ptr exception_ptr = nullptr;
			photon::Awaiter<photon::AutoContext> awaiter;

			auto task = [&result_storage, &exception_ptr, &awaiter, &func]() {
				try {
					if constexpr (std::is_copy_constructible<RetType>::value &&
								  std::is_move_constructible<RetType>::value) {
						result_storage = func();
					}
				} catch (...) {
					exception_ptr = std::current_exception();
				}
				awaiter.resume();
			};

			if (!Add(std::move(task))) {
				throw std::runtime_error("TaskQueue is full");
			}

			awaiter.suspend();

			if (exception_ptr) {
				std::rethrow_exception(exception_ptr);
			}

			return *result_storage;
		}
	}

	void ProcessTasks();

	bool Empty() const { return head_.load() == tail_.load(); }
	int event_fd() const { return event_fd_; }

private:
	static constexpr size_t kCapacity = 4096;
	static constexpr size_t kMask = kCapacity - 1;

	struct Task {
		std::function<void()> func;
		Task() = default;
		template<typename F>
		explicit Task(F&& f) : func(std::forward<F>(f)) {}
	};

	std::atomic<uint64_t> head_{0};
	std::atomic<uint64_t> tail_{0};
	std::unique_ptr<Task[]> buffer_;
	int event_fd_;

	uint64_t increment(uint64_t idx) const {
		return (idx + 1) & kMask;
	}

	bool enqueue(Task&& task);
	bool dequeue(Task& task);
};

template<typename F>
bool TaskQueue::TryAdd(F&& func) {
	return enqueue(Task(std::forward<F>(func)));
}

template<typename F>
bool TaskQueue::Add(F&& func) {
	return TryAdd(std::forward<F>(func));
}
