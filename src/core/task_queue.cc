#include "core/task_queue.h"

#include <photon/common/alog.h>

namespace {
constexpr size_t kMinCapacity = 2;
}

size_t TaskQueue::RoundUpPowerOfTwo(size_t x) {  // 找最接近的2的幂
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

	for (size_t i = 0; i < capacity_; ++i) {
		buffer_[i].sequence.store(i, std::memory_order_relaxed);
	}

	consumer_fibers_.reserve(num_consumers_);
}

TaskQueue::~TaskQueue() {
	Shutdown();

	for (;;) {
		size_t pos = dequeue_pos_.fetch_add(1, std::memory_order_relaxed);
		Cell& cell = buffer_[pos & buffer_mask_];
		size_t seq = cell.sequence.load(std::memory_order_acquire);
		intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
		if (dif < 0) {
			break;
		}
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
			if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
			                                       std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			return false;
		}
	}

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
			if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
			                                       std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			return false;
		}
	}

	CbFunc& src = *reinterpret_cast<CbFunc*>(&cell->storage);
	func = std::move(src);
	src.~CbFunc();

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
		static constexpr uint64_t kMaxBatch = 256;
		uint64_t processed = 0;
		while (processed < kMaxBatch && TryDequeue(func)) {
			func();
			++processed;
		}
		if (processed != 0) {
			// We are actively running; allow a future idle phase to be woken up again.
			wake_pending_.store(false, std::memory_order_release);
			photon::thread_yield();
			continue;
		}

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
			// We are about to block; allow exactly one producer wakeup.
			wake_pending_.store(false, std::memory_order_release);
			pull_sem_.wait(1, kWaitUsec);
			yield_turn = kMaxYieldTurn;
			yield_timeout.timeout(kMaxYieldUsec);
		}

		if (!is_closed_.load(std::memory_order_relaxed)) {
			// We are awake and will execute work; clear wake pending.
			wake_pending_.store(false, std::memory_order_release);
			func();
		}
	}

	while (TryDequeue(func)) {
		func();
	}
}

void TaskQueue::Start(const std::string& base_name) {
	// TaskQueue consumer fibers do not need the default 8MB stack.
	static constexpr uint64_t kConsumerStackSize = 256UL * 1024;  // 256KB
	for (size_t i = 0; i < num_consumers_; ++i) {
		auto* th = photon::thread_create11(kConsumerStackSize, [this]() { Run(); });
		auto* handle = photon::thread_enable_join(th);
		consumer_fibers_.push_back(handle);
	}
}

void TaskQueue::Shutdown() {
	if (is_closed_.exchange(true)) {
		return;
	}

	pull_sem_.signal(num_consumers_);

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
	CbFunc func;
	while (TryDequeue(func)) {
		func();
	}
}
