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

// 测试工具类使用示例
// 这个文件展示了如何使用 test_utils.h 中的工具类

#include "nano_redis/test_utils.h"
#include "nano_redis/arena.h"
#include "nano_redis/status.h"

#include <gtest/gtest.h>
#include <string>

namespace nano_redis {

// Timer 使用示例
TEST(TestUtilsExample, TimerUsage) {
  Timer timer;
  
  // 模拟一些工作
  for (int i = 0; i < 1000; ++i) {
    volatile int x = i * i;  // 防止优化
    (void)x;
  }
  
  double elapsed_ms = timer.ElapsedMillis();
  EXPECT_GT(elapsed_ms, 0.0);
  std::cout << "Operation took " << elapsed_ms << " ms\n";
}

// Benchmark 使用示例
TEST(TestUtilsExample, BenchmarkUsage) {
  Benchmark bench("String Concatenation");
  
  bench.Run([]() {
    std::string s;
    for (int i = 0; i < 100; ++i) {
      s += "test";
    }
  }, 1000);
  
  // 输出示例：
  // String Concatenation          : xxxxx.xx ops/sec, xxxx.xxxx us/op (total: xx.xx ms)
}

// RandomStringGenerator 使用示例
TEST(TestUtilsExample, RandomStringGeneratorUsage) {
  RandomStringGenerator gen(1, 50, 42);  // 固定种子，1-50 字符
  
  std::string s1 = gen.Generate();
  std::string s2 = gen.GenerateFixed(20);
  std::string s3 = gen.GenerateNumber(10);
  
  EXPECT_GE(s1.length(), 1);
  EXPECT_LE(s1.length(), 50);
  EXPECT_EQ(s2.length(), 20);
  EXPECT_EQ(s3.length(), 10);
  
  std::cout << "Random string: " << s1 << "\n";
  std::cout << "Fixed length string: " << s2 << "\n";
  std::cout << "Number string: " << s3 << "\n";
  
  // 使用相同种子应该生成相同的序列
  RandomStringGenerator gen2(1, 50, 42);
  std::string s4 = gen2.Generate();
  EXPECT_EQ(s1, s4);  // 相同种子应该生成相同的字符串
}

// Status 断言宏使用示例
TEST(TestUtilsExample, StatusAssertionMacros) {
  Status ok_status = Status::OK();
  Status not_found = Status::NotFound("key not found");
  
  // 使用自定义断言宏
  ASSERT_OK(ok_status);
  EXPECT_OK(ok_status);
  
  ASSERT_NOT_OK(not_found);
  EXPECT_NOT_OK(not_found);
}

// 使用 Timer 进行性能对比的示例
TEST(TestUtilsExample, PerformanceComparison) {
  Arena arena;
  const size_t kIterations = 10000;
  
  // 测试 1：Arena 分配
  Benchmark bench1("Arena Allocation");
  bench1.Run([&arena]() {
    void* ptr = arena.Allocate(100);
    (void)ptr;  // 避免未使用警告
  }, kIterations);
  
  // 测试 2：标准 malloc
  Benchmark bench2("Standard malloc");
  bench2.Run([]() {
    void* ptr = malloc(100);
    free(ptr);
  }, kIterations);
  
  // 可以直观地看到性能差异
}

}  // namespace nano_redis
