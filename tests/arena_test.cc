#include "nano_redis/arena.h"

#include <cstring>
#include <gtest/gtest.h>

namespace nano_redis {
namespace {

constexpr size_t kDefaultBlockSize = 4096;
constexpr size_t kSmallSize = 32;
constexpr size_t kMediumSize = 256;
constexpr size_t kLargeSize = 4096;

TEST(ArenaTest, DefaultConstruction) {
  Arena arena;
  EXPECT_EQ(0, arena.MemoryUsage());
  EXPECT_EQ(0, arena.BlockCount());
  EXPECT_EQ(0, arena.AllocatedBytes());
}

TEST(ArenaTest, CustomBlockSize) {
  Arena arena(8192);
  EXPECT_EQ(0, arena.MemoryUsage());
  EXPECT_EQ(0, arena.BlockCount());
}

TEST(ArenaTest, AllocateSmallObject) {
  Arena arena;
  void* ptr = arena.Allocate(kSmallSize);
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(1, arena.BlockCount());
  EXPECT_GE(arena.MemoryUsage(), kDefaultBlockSize);
  EXPECT_EQ(kSmallSize, arena.AllocatedBytes());
}

TEST(ArenaTest, AllocateMultipleObjects) {
  Arena arena;
  void* ptr1 = arena.Allocate(kSmallSize);
  void* ptr2 = arena.Allocate(kSmallSize);
  void* ptr3 = arena.Allocate(kSmallSize);
  
  EXPECT_NE(nullptr, ptr1);
  EXPECT_NE(nullptr, ptr2);
  EXPECT_NE(nullptr, ptr3);
  EXPECT_NE(ptr1, ptr2);
  EXPECT_NE(ptr2, ptr3);
  
  EXPECT_EQ(kSmallSize * 3, arena.AllocatedBytes());
}

TEST(ArenaTest, AllocateLargeObject) {
  Arena arena;
  void* ptr = arena.Allocate(kLargeSize);
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(1, arena.BlockCount());
  EXPECT_EQ(kLargeSize, arena.AllocatedBytes());
}

TEST(ArenaTest, AllocationExceedsBlockSize) {
  constexpr size_t kBlockSize = 1024;
  Arena arena(kBlockSize);
  
  arena.Allocate(512);
  arena.Allocate(512);
  EXPECT_EQ(1, arena.BlockCount());
  
  arena.Allocate(512);
  EXPECT_EQ(2, arena.BlockCount());
}

TEST(ArenaTest, Alignment) {
  Arena arena;
  
  void* ptr1 = arena.Allocate(64, 8);
  void* ptr2 = arena.Allocate(64, 16);
  void* ptr3 = arena.Allocate(64, 32);
  void* ptr4 = arena.Allocate(64, 64);
  
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(ptr1) % 8);
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(ptr2) % 16);
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(ptr3) % 32);
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(ptr4) % 64);
}

TEST(ArenaTest, ZeroSizeAllocation) {
  Arena arena;
  void* ptr = arena.Allocate(0);
  EXPECT_EQ(nullptr, ptr);
  EXPECT_EQ(0, arena.AllocatedBytes());
}

TEST(ArenaTest, Reset) {
  Arena arena;
  
  arena.Allocate(kSmallSize);
  arena.Allocate(kMediumSize);
  
  EXPECT_GT(arena.MemoryUsage(), 0);
  EXPECT_GT(arena.AllocatedBytes(), 0);
  
  arena.Reset();
  
  EXPECT_EQ(0, arena.MemoryUsage());
  EXPECT_EQ(0, arena.AllocatedBytes());
  EXPECT_EQ(0, arena.BlockCount());
}

TEST(ArenaTest, MoveConstruction) {
  Arena arena1;
  arena1.Allocate(kSmallSize);
  
  size_t mem_usage = arena1.MemoryUsage();
  
  Arena arena2(std::move(arena1));
  
  EXPECT_EQ(mem_usage, arena2.MemoryUsage());
  EXPECT_EQ(kSmallSize, arena2.AllocatedBytes());
  EXPECT_EQ(0, arena1.MemoryUsage());
  EXPECT_EQ(0, arena1.AllocatedBytes());
}

TEST(ArenaTest, MoveAssignment) {
  Arena arena1;
  Arena arena2;
  
  arena1.Allocate(kSmallSize);
  size_t mem_usage = arena1.MemoryUsage();
  
  arena2 = std::move(arena1);
  
  EXPECT_EQ(mem_usage, arena2.MemoryUsage());
  EXPECT_EQ(kSmallSize, arena2.AllocatedBytes());
}

TEST(ArenaTest, WriteAndRead) {
  Arena arena;
  
  int* value = static_cast<int*>(arena.Allocate(sizeof(int)));
  *value = 42;
  
  EXPECT_EQ(42, *value);
  
  std::string* str = static_cast<std::string*>(arena.Allocate(sizeof(std::string)));
  new (str) std::string("Hello, Arena!");
  
  EXPECT_EQ("Hello, Arena!", *str);
  using std::string;
  str->~string();
}

TEST(ArenaTest, ManySmallAllocations) {
  Arena arena;
  constexpr int kNumAllocs = 1000;
  constexpr size_t kSize = 16;
  
  for (int i = 0; i < kNumAllocs; ++i) {
    void* ptr = arena.Allocate(kSize);
    EXPECT_NE(nullptr, ptr);
    ::memset(ptr, i % 256, kSize);
  }
  
  EXPECT_EQ(kNumAllocs * kSize, arena.AllocatedBytes());
}

}  // namespace
}  // namespace nano_redis
