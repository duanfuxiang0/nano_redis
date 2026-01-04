#include "core/task_queue.h"
#include <photon/thread/thread.h>
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <cstring>
#include <errno.h>

namespace {
constexpr size_t kMinCapacity = 2;
}  // namespace

size_t TaskQueue::RoundUpPowerOfTwo(size_t x) {
	if (x < kMinCapacity) {
		return kMinCapacity;
	}
	size_t p = 1;
	while (p < x) {
		p <<= 1;
	}
	return p;
}

TaskQueue::TaskQueue(size_t capacity) {
	capacity_ = RoundUpPowerOfTwo(capacity);
	mask_ = capacity_ - 1;
	buffer_.reset(new Cell[capacity_]);

	for (size_t i = 0; i < capacity_; ++i) {
		buffer_[i].seq.store(i, std::memory_order_relaxed);
	}

	event_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (event_fd_ < 0) {
		LOG_ERRNO_RETURN(0, , "Failed to create eventfd");
	}
}

TaskQueue::~TaskQueue() {
	if (event_fd_ >= 0) {
		close(event_fd_);
	}
}

bool TaskQueue::enqueue(Task&& task) {
	Cell* cell = nullptr;
	size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

	while (true) {
		cell = &buffer_[pos & mask_];
		const size_t seq = cell->seq.load(std::memory_order_acquire);
		const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

		if (dif == 0) {
			if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			return false;  // full
		} else {
			pos = enqueue_pos_.load(std::memory_order_relaxed);
		}
	}

	cell->task = std::move(task);
	cell->seq.store(pos + 1, std::memory_order_release);

	uint64_t value = 1;
	ssize_t ret = ::write(event_fd_, &value, sizeof(value));
	if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		LOG_ERROR("Failed to write to eventfd: ", errno);
	}

	return true;
}

bool TaskQueue::dequeue(Task& task) {
	Cell* cell = nullptr;
	size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

	while (true) {
		cell = &buffer_[pos & mask_];
		const size_t seq = cell->seq.load(std::memory_order_acquire);
		const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

		if (dif == 0) {
			if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			return false;  // empty
		} else {
			pos = dequeue_pos_.load(std::memory_order_relaxed);
		}
	}

	task = std::move(cell->task);
	cell->seq.store(pos + capacity_, std::memory_order_release);

	return true;
}

void TaskQueue::ProcessTasks() {
	uint64_t buf;
	::read(event_fd_, &buf, sizeof(buf));

	Task task;
	while (dequeue(task)) {
		task.func();
	}
}
