#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include <photon/thread/st.h>
#include <photon/thread/thread.h>
#include <photon/thread/awaiter.h>
#include <photon/common/alog.h>
#include <sys/eventfd.h>
#include <unistd.h>

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
			auto exception_ptr = std::make_unique<std::exception_ptr>();
			auto awaiter = std::make_unique<photon::Awaiter<photon::AutoContext>>();
			auto func_copy = std::make_unique<std::decay_t<F>>(std::forward<F>(func));

			auto task = [exception_ptr_ptr = exception_ptr.get(),
			             awaiter_ptr = awaiter.get(),
			             func_ptr = func_copy.get()]() mutable {
				try {
					(*func_ptr)();
				} catch (...) {
					*exception_ptr_ptr = std::current_exception();
				}
				awaiter_ptr->resume();
			};

			if (!Add(std::move(task))) {
				throw std::runtime_error("TaskQueue is full");
			}

			awaiter->suspend();

			if (*exception_ptr) {
				std::rethrow_exception(*exception_ptr);
			}
		} else {
			static_assert(std::is_copy_constructible<RetType>::value &&
			              std::is_move_constructible<RetType>::value,
			              "TaskQueue::Await return type must be copyable and movable");

			struct State {
				std::optional<RetType> result;
				std::exception_ptr exception;
				photon::Awaiter<photon::AutoContext> awaiter;
				std::decay_t<F> func;

				explicit State(F&& f) : func(std::forward<F>(f)) {}
			};

			auto state = std::make_unique<State>(std::forward<F>(func));

			auto task = [state_ptr = state.get()]() mutable {
				try {
					state_ptr->result = state_ptr->func();
				} catch (...) {
					state_ptr->exception = std::current_exception();
				}
				state_ptr->awaiter.resume();
			};

			if (!Add(std::move(task))) {
				throw std::runtime_error("TaskQueue is full");
			}

			state->awaiter.suspend();

			if (state->exception) {
				std::rethrow_exception(state->exception);
			}

			return *state->result;
		}
	}

	void ProcessTasks();

	bool Empty() const { return enqueue_pos_.load(std::memory_order_relaxed) == dequeue_pos_.load(std::memory_order_relaxed); }
	int event_fd() const { return event_fd_; }

private:
	static size_t RoundUpPowerOfTwo(size_t x);

	struct Task {
		std::function<void()> func;
		Task() = default;
		template<typename F>
		explicit Task(F&& f) : func(std::forward<F>(f)) {}
	};

	struct Cell {
		std::atomic<size_t> seq{0};
		Task task;
	};

	size_t capacity_ = 0;
	size_t mask_ = 0;
	std::unique_ptr<Cell[]> buffer_;

	std::atomic<size_t> enqueue_pos_{0};
	std::atomic<size_t> dequeue_pos_{0};
	int event_fd_;

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
