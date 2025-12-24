// Copyright 2025 Nano-Redis Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "nano_redis/string_utils.h"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace nano_redis {
namespace {

// 简单的计时器类
class Timer {
 public:
  Timer() : start_(std::chrono::high_resolution_clock::now()) {}

  double ElapsedSeconds() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end - start_).count();
  }

 private:
  std::chrono::high_resolution_clock::time_point start_;
};

// 生成随机字符串
std::string GenerateRandomString(size_t length) {
  static const char kChars[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<size_t> dist(0, sizeof(kChars) - 2);

  std::string result;
  result.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    result += kChars[dist(gen)];
  }

  return result;
}

// 基准测试：SSO (Small String Optimization)
void BenchmarkSSO() {
  std::cout << "\n=== SSO (Small String Optimization) Benchmark ===\n";

  // 小字符串（SSO）
  std::vector<std::string> small_strings;
  small_strings.reserve(1000000);

  Timer timer;
  for (int i = 0; i < 1000000; ++i) {
    small_strings.push_back("short");
  }
  double elapsed = timer.ElapsedSeconds();

  std::cout << "Create 1M small strings (SSO): " << elapsed << "s\n";
  std::cout << "SSO Threshold: " << StringUtils::SSOThreshold() << " bytes\n";

  // 检查是否使用了 SSO
  std::string sso_str = "short";
  std::cout << "String 'short' uses SSO: "
            << (StringUtils::IsSSO(sso_str) ? "Yes" : "No") << "\n";

  std::string long_str = "This is a very long string that exceeds SSO threshold";
  std::cout << "Long string uses SSO: "
            << (StringUtils::IsSSO(long_str) ? "Yes" : "No") << "\n";
}

// 基准测试：字符串拷贝优化
void BenchmarkCopyOptimization() {
  std::cout << "\n=== String Copy Optimization Benchmark ===\n";

  const int kIterations = 1000000;

  // 测试 1：值传递 vs 引用传递
  std::string test_str = "Hello, World!";

  Timer timer1;
  for (int i = 0; i < kIterations; ++i) {
    // 值传递（会触发拷贝）
    [[maybe_unused]] std::string copy = test_str;
  }
  double elapsed1 = timer1.ElapsedSeconds();

  Timer timer2;
  for (int i = 0; i < kIterations; ++i) {
    // 引用传递（无拷贝）
    const std::string& ref = test_str;
    [[maybe_unused]] bool empty = ref.empty();
  }
  double elapsed2 = timer2.ElapsedSeconds();

  std::cout << "Value pass (copy): " << elapsed1 << "s\n";
  std::cout << "Reference pass (no copy): " << elapsed2 << "s\n";
  std::cout << "Speedup: " << (elapsed1 / elapsed2) << "x\n";

  // 测试 2：std::move 优化
  Timer timer3;
  std::string dest;
  for (int i = 0; i < kIterations; ++i) {
    std::string src = GenerateRandomString(100);
    dest = src;  // 拷贝
  }
  double elapsed3 = timer3.ElapsedSeconds();

  Timer timer4;
  std::string dest2;
  for (int i = 0; i < kIterations; ++i) {
    std::string src = GenerateRandomString(100);
    dest2 = std::move(src);  // 移动
  }
  double elapsed4 = timer4.ElapsedSeconds();

  std::cout << "\nCopy assignment: " << elapsed3 << "s\n";
  std::cout << "Move assignment: " << elapsed4 << "s\n";
  std::cout << "Speedup: " << (elapsed3 / elapsed4) << "x\n";
}

// 基准测试：字符串操作性能
void BenchmarkStringOperations() {
  std::cout << "\n=== String Operations Benchmark ===\n";

  const int kIterations = 100000;

  // ToLower 原地 vs 拷贝
  std::string test_str = "Hello, World! This is a TEST string.";

  Timer timer1;
  for (int i = 0; i < kIterations; ++i) {
    std::string copy = test_str;
    StringUtils::ToLower(&copy);
  }
  double elapsed1 = timer1.ElapsedSeconds();

  Timer timer2;
  for (int i = 0; i < kIterations; ++i) {
    [[maybe_unused]] std::string lower = StringUtils::ToLowerCopy(test_str);
  }
  double elapsed2 = timer2.ElapsedSeconds();

  std::cout << "ToLower (in-place): " << elapsed1 << "s\n";
  std::cout << "ToLowerCopy (return new): " << elapsed2 << "s\n";

  // Split 性能
  std::string long_str;
  for (int i = 0; i < 100; ++i) {
    long_str += "word" + std::to_string(i) + ",";
  }
  long_str.pop_back();  // 移除最后的逗号

  Timer timer3;
  for (int i = 0; i < kIterations; ++i) {
    [[maybe_unused]] auto parts = StringUtils::Split(long_str, ',');
  }
  double elapsed3 = timer3.ElapsedSeconds();

  std::cout << "Split (100 parts, " << kIterations << "x): " << elapsed3 << "s\n";

  // Join 性能
  std::vector<std::string> parts = StringUtils::Split(long_str, ',');

  Timer timer4;
  for (int i = 0; i < kIterations; ++i) {
    [[maybe_unused]] std::string joined = StringUtils::Join(parts, ",");
  }
  double elapsed4 = timer4.ElapsedSeconds();

  std::cout << "Join (100 parts, " << kIterations << "x): " << elapsed4 << "s\n";
}

// 基准测试：reserve vs 不 reserve
void BenchmarkReserve() {
  std::cout << "\n=== Reserve vs No Reserve Benchmark ===\n";

  const int kIterations = 100000;

  // Concat 不使用 reserve
  std::vector<std::string> parts;
  for (int i = 0; i < 100; ++i) {
    parts.push_back("part" + std::to_string(i));
  }

  Timer timer1;
  for (int i = 0; i < kIterations; ++i) {
    std::string result;
    // 不 reserve
    for (const auto& part : parts) {
      result += part;
    }
  }
  double elapsed1 = timer1.ElapsedSeconds();

  // Concat 使用 reserve
  Timer timer2;
  for (int i = 0; i < kIterations; ++i) {
    [[maybe_unused]] std::string result = StringUtils::Concat(parts);
  }
  double elapsed2 = timer2.ElapsedSeconds();

  std::cout << "Concat without reserve: " << elapsed1 << "s\n";
  std::cout << "Concat with reserve: " << elapsed2 << "s\n";
  std::cout << "Speedup: " << (elapsed1 / elapsed2) << "x\n";
}

// 基准测试：大小写不敏感比较
void BenchmarkCaseInsensitiveCompare() {
  std::cout << "\n=== Case Insensitive Compare Benchmark ===\n";

  const int kIterations = 1000000;

  std::string str1 = "Hello, World!";
  std::string str2 = "hello, world!";

  Timer timer1;
  for (int i = 0; i < kIterations; ++i) {
    [[maybe_unused]] int result = StringUtils::CompareIgnoreCase(str1, str2);
  }
  double elapsed1 = timer1.ElapsedSeconds();

  Timer timer2;
  for (int i = 0; i < kIterations; ++i) {
    [[maybe_unused]] bool starts_with =
        StringUtils::StartsWithIgnoreCase(str1, "hello");
  }
  double elapsed2 = timer2.ElapsedSeconds();

  std::cout << "CompareIgnoreCase (1M): " << elapsed1 << "s\n";
  std::cout << "StartsWithIgnoreCase (1M): " << elapsed2 << "s\n";
}

// 基准测试：内存使用
void BenchmarkMemoryUsage() {
  std::cout << "\n=== Memory Usage Benchmark ===\n";

  const size_t kNumStrings = 100000;

  // 小字符串（SSO）
  std::vector<std::string> small_strings;
  small_strings.reserve(kNumStrings);

  for (size_t i = 0; i < kNumStrings; ++i) {
    small_strings.push_back("short");
  }

  size_t small_usage = 0;
  for (const auto& s : small_strings) {
    small_usage += s.capacity();
  }

  std::cout << "Small strings (SSO, 100K): " << kNumStrings << " strings\n";
  std::cout << "Total capacity: " << small_usage << " bytes\n";
  std::cout << "Average per string: " << (small_usage / kNumStrings) << " bytes\n";

  // 大字符串
  std::vector<std::string> large_strings;
  large_strings.reserve(kNumStrings);

  for (size_t i = 0; i < kNumStrings; ++i) {
    large_strings.push_back(GenerateRandomString(100));
  }

  size_t large_usage = 0;
  for (const auto& s : large_strings) {
    large_usage += s.capacity();
  }

  std::cout << "\nLarge strings (100 chars, 100K): " << kNumStrings << " strings\n";
  std::cout << "Total capacity: " << large_usage << " bytes\n";
  std::cout << "Average per string: " << (large_usage / kNumStrings) << " bytes\n";

  // 内存比例
  double ratio = static_cast<double>(small_usage) / large_usage;
  std::cout << "\nSSO memory ratio: " << (ratio * 100) << "%\n";
}

}  // namespace
}  // namespace nano_redis

int main() {
  std::cout << "========================================\n";
  std::cout << "  String Utils Performance Benchmark  \n";
  std::cout << "========================================\n";

  nano_redis::BenchmarkSSO();
  nano_redis::BenchmarkCopyOptimization();
  nano_redis::BenchmarkStringOperations();
  nano_redis::BenchmarkReserve();
  nano_redis::BenchmarkCaseInsensitiveCompare();
  nano_redis::BenchmarkMemoryUsage();

  std::cout << "\n========================================\n";
  std::cout << "  Benchmark Complete  \n";
  std::cout << "========================================\n";

  return 0;
}
