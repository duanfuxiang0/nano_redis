// Copyright 2024 Nano-Redis Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NANO_REDIS_TEST_UTILS_H_
#define NANO_REDIS_TEST_UTILS_H_

#include "nano_redis/status.h"

#include <chrono>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace nano_redis {

// 测试辅助类：性能计时器
class Timer {
 public:
  Timer() : start_(std::chrono::high_resolution_clock::now()) {}

  // 返回经过的毫秒数
  double ElapsedMillis() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start_).count();
  }

  // 返回经过的微秒数
  double ElapsedMicros() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start_).count();
  }

  // 返回经过的秒数
  double ElapsedSeconds() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end - start_).count();
  }

  // 重置计时器
  void Reset() {
    start_ = std::chrono::high_resolution_clock::now();
  }

 private:
  std::chrono::high_resolution_clock::time_point start_;
};

// 性能测试辅助类
class Benchmark {
 public:
  using BenchmarkFunc = std::function<void()>;

  explicit Benchmark(const std::string& name) : name_(name) {}

  // 运行基准测试
  void Run(BenchmarkFunc func, size_t iterations = 1000) {
    // 预热
    for (size_t i = 0; i < std::min(iterations / 10, size_t(10)); ++i) {
      func();
    }

    // 实际测试
    Timer timer;
    for (size_t i = 0; i < iterations; ++i) {
      func();
    }

    double elapsed_ms = timer.ElapsedMillis();
    double ops_per_sec = (iterations * 1000.0) / elapsed_ms;
    double avg_time_us = (elapsed_ms * 1000.0) / iterations;

    printf("%-40s: %8.2f ops/sec, %8.4f us/op (total: %.2f ms)\n",
           name_.c_str(), ops_per_sec, avg_time_us, elapsed_ms);
  }

  // 运行多次基准测试并统计
  void RunMultiple(BenchmarkFunc func, size_t iterations = 1000,
                   size_t rounds = 5) {
    std::vector<double> times;
    for (size_t round = 0; round < rounds; ++round) {
      Timer timer;
      for (size_t i = 0; i < iterations; ++i) {
        func();
      }
      times.push_back(timer.ElapsedMillis());
    }

    // 计算统计数据
    double sum = 0.0;
    double min_time = times[0];
    double max_time = times[0];
    for (double t : times) {
      sum += t;
      min_time = std::min(min_time, t);
      max_time = std::max(max_time, t);
    }
    double avg_time = sum / times.size();
    double ops_per_sec = (iterations * 1000.0) / avg_time;

    printf("%-40s: %8.2f ops/sec (avg: %.2f ms, min: %.2f ms, max: %.2f ms)\n",
           name_.c_str(), ops_per_sec, avg_time, min_time, max_time);
  }

 private:
  std::string name_;
};

// 随机字符串生成器
class RandomStringGenerator {
 public:
  explicit RandomStringGenerator(size_t min_len = 1, size_t max_len = 100,
                                  int seed = 0)
      : min_len_(min_len),
        max_len_(max_len),
        gen_(seed == 0 ? std::random_device{}() : seed),
        dist_(min_len_, max_len_),
        char_dist_(0, strlen(charset_) - 1) {}

  // 生成随机字符串
  std::string Generate() {
    size_t len = dist_(gen_);
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
      result += charset_[char_dist_(gen_)];
    }
    return result;
  }

  // 生成固定长度的随机字符串
  std::string GenerateFixed(size_t len) {
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
      result += charset_[char_dist_(gen_)];
    }
    return result;
  }

  // 生成随机数字字符串
  std::string GenerateNumber(size_t len) {
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
      result += '0' + (gen_() % 10);
    }
    return result;
  }

 private:
  size_t min_len_;
  size_t max_len_;
  std::mt19937 gen_;
  std::uniform_int_distribution<size_t> dist_;
  std::uniform_int_distribution<size_t> char_dist_;
  static constexpr const char* charset_ =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
};

// 断言辅助宏：检查 Status
#define ASSERT_OK(expr) \
  do { \
    const Status& _s = (expr); \
    ASSERT_TRUE(_s.ok()) << "Status not OK: " << _s.ToString(); \
  } while (0)

#define EXPECT_OK(expr) \
  do { \
    const Status& _s = (expr); \
    EXPECT_TRUE(_s.ok()) << "Status not OK: " << _s.ToString(); \
  } while (0)

#define ASSERT_NOT_OK(expr) \
  do { \
    const Status& _s = (expr); \
    ASSERT_FALSE(_s.ok()) << "Status unexpectedly OK: " << _s.ToString(); \
  } while (0)

#define EXPECT_NOT_OK(expr) \
  do { \
    const Status& _s = (expr); \
    EXPECT_FALSE(_s.ok()) << "Status unexpectedly OK: " << _s.ToString(); \
  } while (0)

// 内存使用辅助函数
class MemoryTracker {
 public:
  MemoryTracker() {
    // 初始化时记录当前内存使用情况
    // 注意：这只是一个近似值，实际使用可能需要平台特定的 API
  }

  // 返回当前进程的内存使用量（近似）
  static size_t GetCurrentMemoryUsage() {
    // 这里只是一个占位实现
    // 在实际项目中，可以使用 /proc/self/status 或平台特定的 API
    return 0;
  }

  // 检查内存是否泄漏（简单的近似）
  bool HasLeaked() const {
    return false;  // 占位实现
  }
};

}  // namespace nano_redis

#endif  // NANO_REDIS_TEST_UTILS_H_
