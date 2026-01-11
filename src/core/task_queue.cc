#include "core/task_queue.h"

#include <photon/common/alog.h>

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

TaskQueue::TaskQueue(size_t queue_size, size_t num_consumers)
    : num_consumers_(num_consumers) {
	capacity_ = RoundUpPowerOfTwo(queue_size);
	buffer_mask_ = capacity_ - 1;
	buffer_.reset(new Cell[capacity_]);

	// Initialize sequence numbers
	for (size_t i = 0; i < capacity_; ++i) {
		buffer_[i].sequence.store(i, std::memory_order_relaxed);
	}

	consumer_fibers_.reserve(num_consumers_);
}

TaskQueue::~TaskQueue() {
	Shutdown();

	// Clean up any remaining items in the queue
	for (;;) {
		size_t pos = dequeue_pos_.fetch_add(1, std::memory_order_relaxed);
		Cell& cell = buffer_[pos & buffer_mask_];
		size_t seq = cell.sequence.load(std::memory_order_acquire);
		intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
		if (dif < 0) {
			break;  // Queue is empty
		}
		// Destroy the function object
		reinterpret_cast<CbFunc*>(&cell.storage)->~CbFunc();
	}
}

bool TaskQueue::TryEnqueue(CbFunc&& func) {
	size_t pos;
	Cell* cell;

	while (true) {
		pos = enqueue_pos_.load(std::memory_order_relaxed);
		cell = &buffer_[pos & buffer_mask_];
		size_t seq = cell->sequence.load(std::memory_order_acquire);
		intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

		if (dif == 0) {
			// Cell is available, try to claim it
			if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
			                                       std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			// Queue is full
			return false;
		}
		// dif > 0: another thread is writing to this cell, retry
	}

	// Construct the function in-place
	new (&cell->storage) CbFunc(std::move(func));
	cell->sequence.store(pos + 1, std::memory_order_release);
	return true;
}

bool TaskQueue::TryDequeue(CbFunc& func) {
	Cell* cell;
	size_t pos;

	while (true) {
		pos = dequeue_pos_.load(std::memory_order_relaxed);
		cell = &buffer_[pos & buffer_mask_];
		size_t seq = cell->sequence.load(std::memory_order_acquire);
		intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

		if (dif == 0) {
			// Cell has data, try to claim it
			if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
			                                       std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			// Queue is empty
			return false;
		}
		// dif > 0: another thread is reading from this cell, retry
	}

	// Move the function out and destroy it
	CbFunc& src = *reinterpret_cast<CbFunc*>(&cell->storage);
	func = std::move(src);
	src.~CbFunc();

	// Release the cell for reuse
	cell->sequence.store(pos + buffer_mask_ + 1, std::memory_order_release);
	return true;
}

void TaskQueue::Run() {
	struct IdlerGuard {
	public:
		explicit IdlerGuard(std::atomic<uint64_t>& idler) : idler_(idler) {
			idler_.fetch_add(1, std::memory_order_acq_rel);
		}
		~IdlerGuard() { idler_.fetch_sub(1, std::memory_order_acq_rel); }

	private:
		std::atomic<uint64_t>& idler_;
	};

	CbFunc func;
	while (!is_closed_.load(std::memory_order_relaxed)) {
		// Fast path: execute a bounded batch, then yield once for fairness.
		// This avoids both extremes:
		// - draining forever without yielding (starvation)
		// - yielding after every task (huge overhead / throughput collapse)
		// Throughput-oriented: allow a larger batch to amortize fiber scheduling costs.
		static constexpr uint64_t kMaxBatch = 256;
		uint64_t processed = 0;
		while (processed < kMaxBatch && TryDequeue(func)) {
			func();
			++processed;
		}
		if (processed != 0) {
			photon::thread_yield();
			continue;
		}

		// Slow path: no work. Follow photon::common::RingChannel's strategy:
		// yield a bit first (helps load balancing, keeps photon::now fresh),
		// then go into semaphore wait with a timeout as a backstop.
		// OPTIMIZED: Drastically reduced delays for better responsiveness under high load
		photon::thread_yield();
		IdlerGuard guard(idler_);

		static constexpr uint64_t kMaxYieldTurn = 4;     // Reduced from 256 to 4
		static constexpr uint64_t kMaxYieldUsec = 100;   // Reduced from 1024 to 100us
		static constexpr uint64_t kWaitUsec = 10000;     // Reduced from 100ms to 10ms

		photon::Timeout yield_timeout(kMaxYieldUsec);
		uint64_t yield_turn = kMaxYieldTurn;
		while (!is_closed_.load(std::memory_order_relaxed) && !TryDequeue(func)) {
			if (yield_turn > 0 && !yield_timeout.expired()) {
				--yield_turn;
				photon::thread_yield();
				continue;
			}
			pull_sem_.wait(1, kWaitUsec);
			yield_turn = kMaxYieldTurn;
			yield_timeout.timeout(kMaxYieldUsec);
		}

		if (!is_closed_.load(std::memory_order_relaxed)) {
			func();
		}
	}

	// Drain remaining tasks after shutdown
	while (TryDequeue(func)) {
		func();
	}
}

void TaskQueue::Start(const std::string& base_name) {
	for (size_t i = 0; i < num_consumers_; ++i) {
		auto* th = photon::thread_create11([this]() { Run(); });
		auto* handle = photon::thread_enable_join(th);
		consumer_fibers_.push_back(handle);
	}
}

void TaskQueue::Shutdown() {
	if (is_closed_.exchange(true)) {
		return;  // Already closed
	}

	// Wake up all consumer fibers
	pull_sem_.signal(num_consumers_);

	// Wait for all consumer fibers to finish
	for (auto* handle : consumer_fibers_) {
		photon::thread_join(handle);
	}
	consumer_fibers_.clear();
}

bool TaskQueue::Empty() const {
	size_t head = dequeue_pos_.load(std::memory_order_relaxed);
	Cell* cell = &buffer_[head & buffer_mask_];
	size_t seq = cell->sequence.load(std::memory_order_acquire);
	intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head + 1);
	return dif < 0;
}

void TaskQueue::ProcessTasks() {
	// Legacy method for backward compatibility
	CbFunc func;
	while (TryDequeue(func)) {
		func();
	}
}
