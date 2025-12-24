#ifndef NANO_REDIS_ARENA_H_
#define NANO_REDIS_ARENA_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nano_redis {

class Arena {
 public:
  Arena();
  explicit Arena(size_t block_size);
  ~Arena();

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  Arena(Arena&& other) noexcept;
  Arena& operator=(Arena&& other) noexcept;

  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
  
  void Reset();
  
  size_t MemoryUsage() const { return memory_usage_; }
  size_t BlockCount() const { return blocks_.size(); }
  size_t AllocatedBytes() const { return allocated_bytes_; }

 private:
  void AllocateNewBlock(size_t min_block_size);

  char* ptr_;
  char* end_;
  size_t block_size_;
  std::vector<void*> blocks_;
  size_t memory_usage_;
  size_t allocated_bytes_;
};

}  // namespace nano_redis

#endif  // NANO_REDIS_ARENA_H_
