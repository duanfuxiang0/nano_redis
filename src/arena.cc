#include "nano_redis/arena.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace nano_redis {

static constexpr size_t kDefaultBlockSize = 4096;

Arena::Arena()
    : ptr_(nullptr),
      end_(nullptr),
      block_size_(kDefaultBlockSize),
      memory_usage_(0),
      allocated_bytes_(0) {}

Arena::Arena(size_t block_size)
    : ptr_(nullptr),
      end_(nullptr),
      block_size_(block_size),
      memory_usage_(0),
      allocated_bytes_(0) {}

Arena::~Arena() {
  for (void* block : blocks_) {
    ::operator delete(block);
  }
}

Arena::Arena(Arena&& other) noexcept
    : ptr_(other.ptr_),
      end_(other.end_),
      block_size_(other.block_size_),
      blocks_(std::move(other.blocks_)),
      memory_usage_(other.memory_usage_),
      allocated_bytes_(other.allocated_bytes_) {
  other.ptr_ = nullptr;
  other.end_ = nullptr;
  other.memory_usage_ = 0;
  other.allocated_bytes_ = 0;
}

Arena& Arena::operator=(Arena&& other) noexcept {
  if (this != &other) {
    for (void* block : blocks_) {
      ::operator delete(block);
    }
    
    ptr_ = other.ptr_;
    end_ = other.end_;
    block_size_ = other.block_size_;
    blocks_ = std::move(other.blocks_);
    memory_usage_ = other.memory_usage_;
    allocated_bytes_ = other.allocated_bytes_;
    
    other.ptr_ = nullptr;
    other.end_ = nullptr;
    other.memory_usage_ = 0;
    other.allocated_bytes_ = 0;
  }
  return *this;
}

void* Arena::Allocate(size_t size, size_t alignment) {
  if (size == 0) {
    return nullptr;
  }

  size_t offset = reinterpret_cast<uintptr_t>(ptr_) % alignment;
  size_t padding = (offset == 0) ? 0 : alignment - offset;

  size_t total_size = size + padding;
  
  if (ptr_ + total_size > end_) {
    AllocateNewBlock(std::max(total_size, block_size_));
    
    offset = reinterpret_cast<uintptr_t>(ptr_) % alignment;
    padding = (offset == 0) ? 0 : alignment - offset;
    total_size = size + padding;
  }

  char* result = ptr_ + padding;
  ptr_ += total_size;
  allocated_bytes_ += size;

  return result;
}

void Arena::Reset() {
  ptr_ = nullptr;
  end_ = nullptr;
  allocated_bytes_ = 0;
  
  for (void* block : blocks_) {
    ::operator delete(block);
  }
  blocks_.clear();
  memory_usage_ = 0;
}

void Arena::AllocateNewBlock(size_t min_block_size) {
  size_t block_size = std::max(min_block_size, block_size_);
  
  char* block = static_cast<char*>(::operator new(block_size));
  blocks_.push_back(block);
  
  ptr_ = block;
  end_ = block + block_size;
  
  memory_usage_ += block_size;
}

}  // namespace nano_redis
