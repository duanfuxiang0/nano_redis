#include "core/task_queue.h"
#include <photon/thread/thread.h>
#include <photon/common/alog.h>
#include <photon/common/alog-stdstring.h>
#include <cstring>
#include <errno.h>

TaskQueue::TaskQueue(size_t capacity) : buffer_(new Task[kCapacity]) {
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
	uint64_t pos = tail_.load(std::memory_order_relaxed);
	uint64_t next_pos = increment(pos);

	if (next_pos == head_.load(std::memory_order_acquire)) {
		return false;
	}

	buffer_[pos] = std::move(task);
	tail_.store(next_pos, std::memory_order_release);

	uint64_t value = 1;
	ssize_t ret = ::write(event_fd_, &value, sizeof(value));
	if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		LOG_ERROR("Failed to write to eventfd: ", errno);
	}

	return true;
}

bool TaskQueue::dequeue(Task& task) {
	uint64_t head = head_.load(std::memory_order_relaxed);
	uint64_t tail = tail_.load(std::memory_order_acquire);

	if (head == tail) {
		return false;
	}

	task = std::move(buffer_[head]);
	head_.store(increment(head), std::memory_order_release);

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
