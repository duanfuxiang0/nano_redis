#include "nano_redis/arena.h"

#include <benchmark/benchmark.h>

#include <cstring>
#include <random>

namespace nano_redis {
namespace {

constexpr size_t kSmallSize = 32;
constexpr size_t kMediumSize = 256;
constexpr size_t kLargeSize = 1024;

static void BM_Arena_SmallAllocations(benchmark::State& state) {
  Arena arena(1024 * 1024);
  
  for (auto _ : state) {
    void* ptr = arena.Allocate(kSmallSize);
    benchmark::DoNotOptimize(ptr);
  }
  
  state.SetBytesProcessed(state.iterations() * kSmallSize);
}

static void BM_Malloc_SmallAllocations(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = ::malloc(kSmallSize);
    benchmark::DoNotOptimize(ptr);
    ::free(ptr);
  }
  
  state.SetBytesProcessed(state.iterations() * kSmallSize);
}

static void BM_Arena_MixedAllocations(benchmark::State& state) {
  Arena arena(1024 * 1024);
  
  std::vector<size_t> sizes = {kSmallSize, kMediumSize, kLargeSize, kSmallSize};
  size_t idx = 0;
  
  for (auto _ : state) {
    void* ptr = arena.Allocate(sizes[idx % sizes.size()]);
    benchmark::DoNotOptimize(ptr);
    ++idx;
  }
}

static void BM_Malloc_MixedAllocations(benchmark::State& state) {
  std::vector<size_t> sizes = {kSmallSize, kMediumSize, kLargeSize, kSmallSize};
  size_t idx = 0;
  
  for (auto _ : state) {
    void* ptr = ::malloc(sizes[idx % sizes.size()]);
    benchmark::DoNotOptimize(ptr);
    ::free(ptr);
    ++idx;
  }
}

static void BM_Arena_AllocationAndReset(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    Arena arena(1024 * 1024);
    state.ResumeTiming();
    
    for (int i = 0; i < 1000; ++i) {
      void* ptr = arena.Allocate(kSmallSize);
      benchmark::DoNotOptimize(ptr);
    }
    
    arena.Reset();
  }
}

static void BM_Malloc_AllocationAndFree(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<void*> ptrs;
    ptrs.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
      ptrs.push_back(::malloc(kSmallSize));
    }
    
    for (void* ptr : ptrs) {
      ::free(ptr);
    }
  }
}

static void BM_Arena_StringAllocation(benchmark::State& state) {
  Arena arena(1024 * 1024);
  const std::string test_str = "Hello, Arena Benchmark!";
  
  for (auto _ : state) {
    void* ptr = arena.Allocate(sizeof(std::string));
    benchmark::DoNotOptimize(ptr);
  }
}

static void BM_Malloc_StringAllocation(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = ::malloc(sizeof(std::string));
    benchmark::DoNotOptimize(ptr);
    ::free(ptr);
  }
}

BENCHMARK(BM_Arena_SmallAllocations);
BENCHMARK(BM_Malloc_SmallAllocations);
BENCHMARK(BM_Arena_MixedAllocations);
BENCHMARK(BM_Malloc_MixedAllocations);
BENCHMARK(BM_Arena_AllocationAndReset);
BENCHMARK(BM_Malloc_AllocationAndFree);
BENCHMARK(BM_Arena_StringAllocation);
BENCHMARK(BM_Malloc_StringAllocation);

}  // namespace
}  // namespace nano_redis
