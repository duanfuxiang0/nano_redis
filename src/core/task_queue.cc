#include "core/task_queue.h"

#include <photon/common/alog.h>

namespace {

constexpr size_t kMinCapacity = 2;
constexpr uint64_t kMaxBatch = 256;
constexpr uint64_t kMaxYieldTurn = 4;
constexpr uint64_t kMaxYieldUsec = 100;
constexpr uint64_t kWaitUsec = 10000;
constexpr uint64_t kConsumerStackSize = 256UL * 1024;

// RAII guard for tracking idle consumers
class IdlerGuard {
public:
	explicit IdlerGuard(std::atomic<uint64_t>& idler_value) : idler(idler_value) {
		idler.fetch_add(1, std::memory_order_acq_rel);
	}
	~IdlerGuard() {
		idler.fetch_sub(1, std::memory_order_acq_rel);
	}

private:
	std::atomic<uint64_t>& idler;
};

} // namespace

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

TaskQueue::TaskQueue(size_t queue_size, size_t num_consumers_value) : num_consumers(num_consumers_value) {
	capacity = RoundUpPowerOfTwo(queue_size);
	buffer_mask = capacity - 1;
	buffer.reset(new Cell[capacity]);
	for (size_t i = 0; i < capacity; ++i) {
		buffer[i].sequence.store(i, std::memory_order_relaxed);
	}
	consumer_fibers.reserve(num_consumers);
}

TaskQueue::~TaskQueue() {
	Shutdown();
	// Drain remaining tasks
	for (;;) {
		size_t pos = dequeue_pos.fetch_add(1, std::memory_order_relaxed);
		Cell& cell = buffer[pos & buffer_mask];
		size_t seq = cell.sequence.load(std::memory_order_acquire);
		if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1) < 0) {
			break;
		}
		reinterpret_cast<CbFunc*>(&cell.storage)->~CbFunc();
	}
}

bool TaskQueue::TryEnqueue(CbFunc&& func) {
	size_t pos;
	Cell* cell;
	for (;;) {
		pos = enqueue_pos.load(std::memory_order_relaxed);
		cell = &buffer[pos & buffer_mask];
		size_t seq = cell->sequence.load(std::memory_order_acquire);
		intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
		if (dif == 0) {
			if (enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
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
	for (;;) {
		pos = dequeue_pos.load(std::memory_order_relaxed);
		cell = &buffer[pos & buffer_mask];
		size_t seq = cell->sequence.load(std::memory_order_acquire);
		intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
		if (dif == 0) {
			if (dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
				break;
			}
		} else if (dif < 0) {
			return false;
		}
	}
	CbFunc& src = *reinterpret_cast<CbFunc*>(&cell->storage);
	func = std::move(src);
	// NOLINTNEXTLINE(bugprone-use-after-move)
	src.~CbFunc();
	cell->sequence.store(pos + buffer_mask + 1, std::memory_order_release);
	return true;
}

void TaskQueue::Run() {
	CbFunc func;
	while (!is_closed.load(std::memory_order_relaxed)) {
		// Active phase: process a batch of tasks
		uint64_t processed = 0;
		while (processed < kMaxBatch && TryDequeue(func)) {
			func();
			++processed;
		}
		if (processed != 0) {
			wake_pending.store(false, std::memory_order_release);
			photon::thread_yield();
			continue;
		}

		// Idle phase: wait for new tasks
		photon::thread_yield();
		IdlerGuard guard(idler);

		photon::Timeout yield_timeout(kMaxYieldUsec);
		uint64_t yield_turn = kMaxYieldTurn;
		while (!is_closed.load(std::memory_order_relaxed) && !TryDequeue(func)) {
			if (yield_turn > 0 && !yield_timeout.expired()) {
				--yield_turn;
				photon::thread_yield();
				continue;
			}
			wake_pending.store(false, std::memory_order_release);
			pull_sem.wait(1, kWaitUsec);
			yield_turn = kMaxYieldTurn;
			yield_timeout.timeout(kMaxYieldUsec);
		}

		if (!is_closed.load(std::memory_order_relaxed)) {
			wake_pending.store(false, std::memory_order_release);
			func();
		}
	}
	// Drain on shutdown
	while (TryDequeue(func)) {
		func();
	}
}

void TaskQueue::Start(const std::string& base_name) {
	(void)base_name;
	for (size_t i = 0; i < num_consumers; ++i) {
		auto* th = photon::thread_create11(kConsumerStackSize, [this]() { Run(); });
		consumer_fibers.push_back(photon::thread_enable_join(th));
	}
}

void TaskQueue::Shutdown() {
	if (is_closed.exchange(true)) {
		return;
	}
	pull_sem.signal(num_consumers);
	for (auto* handle : consumer_fibers) {
		photon::thread_join(handle);
	}
	consumer_fibers.clear();
}

bool TaskQueue::Empty() const {
	size_t head = dequeue_pos.load(std::memory_order_relaxed);
	Cell* cell = &buffer[head & buffer_mask];
	size_t seq = cell->sequence.load(std::memory_order_acquire);
	return static_cast<intptr_t>(seq) - static_cast<intptr_t>(head + 1) < 0;
}

void TaskQueue::ProcessTasks() {
	CbFunc func;
	while (TryDequeue(func)) {
		func();
	}
}
