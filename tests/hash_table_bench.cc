#include "nano_redis/string_store.h"

#include <benchmark/benchmark.h>
#include <random>
#include <string>

namespace nano_redis {

static constexpr size_t kSmallDataSize = 1000;
static constexpr size_t kMediumDataSize = 10000;
static constexpr size_t kLargeDataSize = 100000;

static constexpr size_t kSmallStringSize = 8;
static constexpr size_t kMediumStringSize = 32;
static constexpr size_t kLargeStringSize = 128;

std::string GenerateString(size_t length, std::mt19937& rng) {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::string result;
  result.reserve(length);
  std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
  for (size_t i = 0; i < length; ++i) {
    result += charset[dist(rng)];
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> GenerateTestData(
    size_t num_items, size_t key_size, size_t value_size) {
  std::mt19937 rng(42);
  std::vector<std::pair<std::string, std::string>> data;
  data.reserve(num_items);
  for (size_t i = 0; i < num_items; ++i) {
    data.emplace_back(GenerateString(key_size, rng), GenerateString(value_size, rng));
  }
  return data;
}

template <typename StoreType>
static void BM_Insert(benchmark::State& state) {
  auto data = GenerateTestData(state.range(0), state.range(1), state.range(2));
  for (auto _ : state) {
    StoreType store;
    for (const auto& [key, value] : data) {
      store.Put(key, value);
    }
    benchmark::DoNotOptimize(store);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename StoreType>
static void BM_InsertMove(benchmark::State& state) {
  auto data = GenerateTestData(state.range(0), state.range(1), state.range(2));
  for (auto _ : state) {
    StoreType store;
    for (auto& [key, value] : data) {
      store.Put(std::move(key), std::move(value));
    }
    benchmark::DoNotOptimize(store);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename StoreType>
static void BM_Lookup(benchmark::State& state) {
  auto data = GenerateTestData(state.range(0), state.range(1), state.range(2));
  StoreType store;
  for (const auto& [key, value] : data) {
    store.Put(key, value);
  }
  
  std::mt19937 rng(42);
  std::uniform_int_distribution<size_t> dist(0, data.size() - 1);
  std::string value;
  
  for (auto _ : state) {
    benchmark::DoNotOptimize(value);
    size_t idx = dist(rng);
    store.Get(data[idx].first, &value);
  }
  state.SetItemsProcessed(state.iterations());
}

template <typename StoreType>
static void BM_Delete(benchmark::State& state) {
  auto data = GenerateTestData(state.range(0), state.range(1), state.range(2));
  
  for (auto _ : state) {
    state.PauseTiming();
    StoreType store;
    for (const auto& [key, value] : data) {
      store.Put(key, value);
    }
    state.ResumeTiming();
    
    for (const auto& [key, _] : data) {
      store.Delete(key);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename StoreType>
static void BM_Iterate(benchmark::State& state) {
  auto data = GenerateTestData(state.range(0), state.range(1), state.range(2));
  StoreType store;
  for (const auto& [key, value] : data) {
    store.Put(key, value);
  }
  
  for (auto _ : state) {
    size_t count = 0;
    for (const auto& [key, value] : store.GetStore()) {
      benchmark::DoNotOptimize(key);
      benchmark::DoNotOptimize(value);
      ++count;
    }
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK_TEMPLATE(BM_Insert, StringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kMediumStringSize, kMediumStringSize})
    ->Args({kMediumDataSize, kLargeStringSize, kLargeStringSize});

BENCHMARK_TEMPLATE(BM_Insert, StdStringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kMediumStringSize, kMediumStringSize})
    ->Args({kMediumDataSize, kLargeStringSize, kLargeStringSize});

BENCHMARK_TEMPLATE(BM_InsertMove, StringStore)
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize});

BENCHMARK_TEMPLATE(BM_InsertMove, StdStringStore)
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize});

BENCHMARK_TEMPLATE(BM_Lookup, StringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kMediumStringSize, kMediumStringSize})
    ->Args({kMediumDataSize, kLargeStringSize, kLargeStringSize});

BENCHMARK_TEMPLATE(BM_Lookup, StdStringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kMediumStringSize, kMediumStringSize})
    ->Args({kMediumDataSize, kLargeStringSize, kLargeStringSize});

BENCHMARK_TEMPLATE(BM_Delete, StringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize});

BENCHMARK_TEMPLATE(BM_Delete, StdStringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize});

BENCHMARK_TEMPLATE(BM_Iterate, StringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize});

BENCHMARK_TEMPLATE(BM_Iterate, StdStringStore)
    ->Args({kSmallDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kMediumDataSize, kSmallStringSize, kSmallStringSize})
    ->Args({kLargeDataSize, kSmallStringSize, kSmallStringSize});

}  // namespace nano_redis

BENCHMARK_MAIN();
