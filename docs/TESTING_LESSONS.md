# 测试框架学习要点

## 1. GoogleTest 基础知识

### 1.1 断言类型

GoogleTest 提供两类断言：

**EXPECT_* 系列**：测试失败后继续执行
```cpp
EXPECT_EQ(x, y);       // x == y
EXPECT_NE(x, y);       // x != y
EXPECT_TRUE(condition);  // condition 为真
EXPECT_FALSE(condition); // condition 为假
```

**ASSERT_* 系列**：测试失败后停止当前测试
```cpp
ASSERT_EQ(x, y);       // x == y，失败则终止测试
ASSERT_NE(x, y);       // x != y
ASSERT_TRUE(condition);
ASSERT_FALSE(condition);
```

**何时使用哪个？**
- 使用 `EXPECT_*` 当你想收集多个失败信息
- 使用 `ASSERT_*` 当后续测试依赖于前面的结果

### 1.2 字符串比较

```cpp
// C 字符串
EXPECT_STREQ(str1, str2);   // 相等
EXPECT_STRNE(str1, str2);   // 不相等

// std::string
EXPECT_EQ(str1, str2);      // 直接使用 EXPECT_EQ
```

### 1.3 浮点数比较

```cpp
EXPECT_DOUBLE_EQ(a, b);     // 近似相等（使用 ULPs）
EXPECT_NEAR(a, b, 0.0001);  // 绝对误差小于 0.0001
```

### 1.4 测试 Fixture

当你有多个测试共享相同的初始化代码时：

```cpp
class ArenaTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 每个测试运行前调用
    arena_ = std::make_unique<Arena>();
  }

  void TearDown() override {
    // 每个测试运行后调用
    arena_.reset();
  }

  std::unique_ptr<Arena> arena_;
};

TEST_F(ArenaTest, AllocateSmallObject) {
  void* ptr = arena_->Allocate(100);
  EXPECT_NE(ptr, nullptr);
}

TEST_F(ArenaTest, AllocateLargeObject) {
  void* ptr = arena_->Allocate(1000000);
  EXPECT_NE(ptr, nullptr);
}
```

## 2. 测试驱动开发 (TDD)

### 2.1 TDD 三步循环

```
┌─────────┐
│   Red   │  编写失败的测试
└────┬────┘
     │
     ▼
┌─────────┐
│  Green  │  编写最小代码使测试通过
└────┬────┘
     │
     ▼
┌─────────┐
│Refactor │  重构改进代码
└────┬────┘
     │
     └─── 回到 Red
```

### 2.2 实战示例

**场景：实现 Arena::Allocate**

**步骤 1：Red - 编写测试**
```cpp
TEST(ArenaTest, AllocateObject) {
  Arena arena;
  void* ptr = arena.Allocate(100);
  EXPECT_NE(ptr, nullptr);
}
```
运行测试：❌ 失败（函数未实现）

**步骤 2：Green - 最小实现**
```cpp
void* Arena::Allocate(size_t size) {
  return malloc(size);  // 最简单的实现
}
```
运行测试：✅ 通过

**步骤 3：Refactor - 改进实现**
```cpp
void* Arena::Allocate(size_t size, size_t alignment) {
  size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
  
  if (ptr_ + aligned_size > end_) {
    AllocateNewBlock(std::max(aligned_size, block_size_));
  }
  
  void* result = ptr_;
  ptr_ += aligned_size;
  return result;
}
```
运行测试：✅ 通过

### 2.3 TDD 的好处

1. **更好的设计**
   - 迫使你先考虑如何使用代码
   - 促进更清晰的接口
   - 更容易发现设计问题

2. **持续的信心**
   - 修改代码时立即知道是否破坏了什么
   - 重构变得更安全
   - 减少回归错误

3. **活文档**
   - 测试展示了代码的使用方式
   - 比注释更可靠
   - 自动更新

## 3. 基准测试技巧

### 3.1 高精度计时

使用 `std::chrono::high_resolution_clock`：

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
// ... 执行操作 ...
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Time: " << duration.count() << " us\n";
```

### 3.2 预热阶段

避免冷启动影响：

```cpp
// 预热 10 次
for (int i = 0; i < 10; ++i) {
  func();
}

// 实际测量
Timer timer;
for (int i = 0; i < iterations; ++i) {
  func();
}
double elapsed = timer.ElapsedMillis();
```

**为什么需要预热？**
- CPU 缓存预热
- 分支预测优化
- 减少冷启动噪声

### 3.3 多次运行统计

减少系统噪声：

```cpp
const int kRounds = 5;
std::vector<double> times;

for (int round = 0; round < kRounds; ++round) {
  Timer timer;
  for (int i = 0; i < iterations; ++i) {
    func();
  }
  times.push_back(timer.ElapsedMillis());
}

// 计算统计数据
double sum = std::accumulate(times.begin(), times.end(), 0.0);
double avg = sum / times.size();
double min = *std::min_element(times.begin(), times.end());
double max = *std::max_element(times.begin(), times.end());
```

### 3.4 性能指标

常用指标：

```cpp
double ops_per_sec = (iterations * 1000.0) / elapsed_ms;
double avg_time_us = (elapsed_ms * 1000.0) / iterations;
double throughput = total_bytes / elapsed_sec;
```

## 4. 测试最佳实践

### 4.1 测试命名

清晰描述测试场景：

```cpp
// ✅ 好
TEST(ArenaTest, AllocateSmallObjectReturnsValidPointer)
TEST(ArenaTest, AllocateLargeObjectAllocatesNewBlock)

// ❌ 差
TEST(ArenaTest, Test1)
TEST(ArenaTest, Allocate)
```

### 4.2 One Assertion Per Test

每个测试只验证一件事：

```cpp
// ✅ 好
TEST(ArenaTest, AllocateReturnsNonNullPointer) {
  Arena arena;
  void* ptr = arena.Allocate(100);
  EXPECT_NE(ptr, nullptr);
}

TEST(ArenaTest, AllocatedMemoryIsAccessible) {
  Arena arena;
  char* ptr = static_cast<char*>(arena.Allocate(100));
  ptr[0] = 'x';  // 不应该崩溃
  EXPECT_EQ(ptr[0], 'x');
}

// ❌ 差
TEST(ArenaTest, Allocate) {
  Arena arena;
  void* ptr = arena.Allocate(100);
  EXPECT_NE(ptr, nullptr);
  static_cast<char*>(ptr)[0] = 'x';  // 两个断言混在一起
  EXPECT_EQ(static_cast<char*>(ptr)[0], 'x');
}
```

### 4.3 测试独立性

测试之间不应该有依赖：

```cpp
// ✅ 好 - 每个测试独立设置
TEST(ArenaTest, Test1) {
  Arena arena;
  // 使用独立的 arena
}

TEST(ArenaTest, Test2) {
  Arena arena;  // 新的 arena，不依赖 Test1
  // 独立测试
}

// ❌ 差 - 测试有依赖
static Arena* g_arena;

TEST(ArenaTest, Setup) {
  g_arena = new Arena();
  // 设置全局状态
}

TEST(ArenaTest, UseSetup) {
  g_arena->Allocate(100);  // 依赖 Setup
}
```

### 4.4 测试边界条件

重点测试边界情况：

```cpp
// ✅ 测试边界
TEST(ArenaTest, AllocateZeroBytes) {
  Arena arena;
  void* ptr = arena.Allocate(0);
  EXPECT_NE(ptr, nullptr);  // 如何处理 0 字节？
}

TEST(ArenaTest, AllocateVeryLargeObject) {
  Arena arena;
  void* ptr = arena.Allocate(1ULL << 40);  // 1TB
  EXPECT_EQ(ptr, nullptr);  // 应该失败
}

TEST(ArenaTest, AllocateExactlyBlockSize) {
  Arena arena;
  size_t block_size = 4096;
  void* ptr = arena.Allocate(block_size);
  EXPECT_NE(ptr, nullptr);
}
```

### 4.5 使用参数化测试

当测试多个相似场景时：

```cpp
class ArenaAllocateTest : public ::testing::TestWithParam<size_t> {};

TEST_P(ArenaAllocateTest, AllocateDifferentSizes) {
  size_t size = GetParam();
  Arena arena;
  void* ptr = arena.Allocate(size);
  EXPECT_NE(ptr, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    VariousSizes,
    ArenaAllocateTest,
    ::testing::Values(1, 10, 100, 1000, 10000)
);
```

## 5. 调试测试

### 5.1 查看详细输出

```bash
# 运行测试并显示详细输出
./test --gtest_verbose

# 列出所有测试但不运行
./test --gtest_list_tests

# 只运行特定测试
./test --gtest_filter=ArenaTest.AllocateSmallObject

# 运行特定测试套件
./test --gtest_filter=ArenaTest.*
```

### 5.2 重复失败的测试

```bash
# 重复运行失败的测试 100 次
./test --gtest_repeat=100 --gtest_break_on_failure

# 在调试器中运行
gdb --args ./test --gtest_filter=ArenaTest.AllocateSmallObject
```

### 5.3 使用日志

```cpp
TEST(ArenaTest, ComplexTest) {
  Arena arena;
  
  LOG(INFO) << "Starting allocation";
  void* ptr = arena.Allocate(100);
  
  LOG(INFO) << "Allocated at: " << ptr;
  EXPECT_NE(ptr, nullptr);
  
  LOG(INFO) << "Test completed";
}
```

## 6. 性能分析

### 6.1 识别热点

使用 perf：

```bash
# CPU 性能分析
perf record -g ./arena_test
perf report

# 缓存 miss 分析
perf stat -e cache-references,cache-misses ./arena_test

# 系统调用统计
perf stat -e syscalls:sys_enter_read,syscalls:sys_enter_write ./arena_test
```

### 6.2 内存分析

使用 Valgrind：

```bash
# 内存泄漏检测
valgrind --leak-check=full ./arena_test

# 缓存分析
valgrind --tool=cachegrind ./arena_test
```

### 6.3 ASan 使用

```bash
# 编译时启用 AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..

# 运行测试
./arena_test
```

## 7. 常见陷阱

### 7.1 忘记初始化

```cpp
TEST(ArenaTest, UseBeforeInit) {
  Arena arena;
  arena.Allocate(100);  // 如果没有初始化会崩溃
}
```

### 7.2 测试本身有 bug

```cpp
TEST(ArenaTest, WrongExpectation) {
  Arena arena;
  void* ptr = arena.Allocate(100);
  EXPECT_EQ(ptr, nullptr);  // 错误的期望！应该是 EXPECT_NE
}
```

### 7.3 测试依赖于随机数

```cpp
TEST(ArenaTest, RandomFailure) {
  Arena arena;
  int random_value = rand();  // 不可预测的结果
  // 测试可能有时通过，有时失败
}
```

**解决方法**：使用固定种子

```cpp
srand(42);  // 固定种子
```

### 7.4 忽略测试

```cpp
TEST(ArenaTest, DisabledTest) {
  // 这个测试被禁用，不会运行
  EXPECT_EQ(1, 2);
}
// 在运行时启用：./test --gtest_also_run_disabled_tests
```

更好的做法是使用 `DISABLED_` 前缀：

```cpp
TEST(ArenaTest, DISABLED_TemporaryTest) {
  // 明确表示这是临时禁用的
}
```

## 8. 进阶技巧

### 8.1 Mock 外部依赖

使用 Google Mock：

```cpp
class MockStorage : public Storage {
 public:
  MOCK_METHOD(Status, Read, (const std::string& key, std::string* value), (override));
  MOCK_METHOD(Status, Write, (const std::string& key, const std::string& value), (override));
};

TEST(DatabaseTest, UseMockStorage) {
  MockStorage mock_storage;
  EXPECT_CALL(mock_storage, Read("key", _))
      .WillOnce(Return(Status::OK()));

  Database db(&mock_storage);
  Status s = db.Get("key");
  EXPECT_OK(s);
}
```

### 8.2 测试异常

```cpp
TEST(ArenaTest, ThrowsOnInvalidSize) {
  Arena arena;
  EXPECT_THROW(arena.Allocate(-1), std::invalid_argument);
}
```

### 8.3 测试时间相关代码

```cpp
TEST(TimerTest, MeasuresCorrectTime) {
  // 使用 mock clock
  MockClock clock;
  EXPECT_CALL(clock, Now())
      .WillOnce(Return(TimePoint(100)))
      .WillOnce(Return(TimePoint(200)));

  Timer timer(&clock);
  // ... 测试 ...
}
```

## 9. 总结

### 关键要点

1. **测试先行**：TDD 改善设计和质量
2. **测试独立性**：每个测试应该独立运行
3. **测试可读性**：测试名称和代码应该清晰
4. **边界测试**：重点测试边界情况
5. **性能测试**：量化性能，避免退化
6. **持续集成**：在 CI 中运行测试

### 下一步

- 学习 Google Mock
- 探索模糊测试 (Fuzzing)
- 了解代码覆盖率工具
- 研究 CI/CD 集成

### 推荐资源

- [GoogleTest 文档](https://google.github.io/googletest/)
- [Google Benchmark 文档](https://github.com/google/benchmark)
- 《C++ 单元测试：GoogleTest 和 GoogleMock》
- 《测试驱动开发：实战指南》

记住：**好的测试是可维护代码的基础！**
