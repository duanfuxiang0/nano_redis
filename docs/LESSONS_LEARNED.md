# Commit 1: 学习要点

## C++ 基础

### 1. 头文件包含保护

```cpp
#ifndef NANO_REDIS_VERSION_H_
#define NANO_REDIS_VERSION_H_

// ... 内容 ...

#endif  // NANO_REDIS_VERSION_H_
```

**为什么需要？**
- 防止多次包含导致的重复定义错误
- 标准的 C/C++ 实践

**替代方案**：
```cpp
#pragma once  // 非标准，但广泛支持
```

**我们选择 #ifndef 因为**：
- 完全可移植
- 明确的保护标识符

### 2. constexpr 编译期常量

```cpp
constexpr int kMajorVersion = 0;
constexpr const char* kVersionString = "0.1.0";
```

**优点**：
- 编译期确定，无运行时开销
- 可用于模板参数
- 优化器可以充分优化

**vs const**：
```cpp
const int kMajorVersion = 0;  // 运行时常量（可能分配内存）
constexpr int kMajorVersion = 0;  // 编译时常量（无内存分配）
```

### 3. inline 函数

```cpp
inline const char* GetVersion() { return kVersionString; }
```

**为什么 inline？**
- 避免链接时的"未定义引用"错误
- 允许编译器内联优化（零函数调用开销）
- 头文件中定义的必要修饰符

**编译器可能的行为**：
- 完全内联：生成 `return "0.1.0";`
- 不内联：生成函数调用

### 4. enum class

```cpp
enum class StatusCode : int {
  kOk = 0,
  kNotFound = 1,
};
```

**vs 传统 enum**：

```cpp
enum StatusCode {  // 无作用域
  kOk = 0,
  kNotFound = 1,
};

// 可以这样写（不安全）：
int x = kOk;  // 隐式转换

enum class StatusCode {  // 有作用域
  kOk = 0,
};

// 必须这样写（安全）：
StatusCode code = StatusCode::kOk;
int x = static_cast<int>(code);  // 显式转换
```

## 项目组织

### 1. 命名空间

```cpp
namespace nano_redis {
  constexpr int kMajorVersion = 0;
  class Status { ... };
}

// 使用：
nano_redis::kMajorVersion
nano_redis::Status s;
```

**为什么需要命名空间？**
- 避免全局符号冲突
- 逻辑组织代码
- 清晰的 API 边界

### 2. Apache License 头注释

```cpp
// Copyright 2024 Nano-Redis Authors
//
// Licensed under the Apache License, Version 2.0 ...
```

**重要性**：
- 法律合规
- 明确版权
- 便于第三方使用（特别是公司）

## 测试

### 1. GoogleTest 基础

```cpp
TEST(VersionTest, GetVersionReturnsCorrectString) {
  EXPECT_EQ(GetVersion(), "0.1.0");
}
```

**命名约定**：
- `TEST(测试套件, 测试名称)`
- `EXPECT_*`: 失败继续执行
- `ASSERT_*`: 失败立即停止

### 2. 测试覆盖

```cpp
// 正常路径
TEST(StatusTest, OKStatus) { ... }

// 错误路径
TEST(StatusTest, NotFoundStatus) { ... }
TEST(StatusTest, InvalidArgumentStatus) { ... }
```

**目标**: 所有公开 API 都有对应测试

## 构建系统

### 1. CMake vs Bazel

| 特性 | CMake | Bazel |
|------|--------|--------|
| 学习曲线 | 中等 | 陡峭 |
| 速度 | 中等 | 快（增量） |
| 跨语言 | 有限 | 优秀 |
| IDE 支持 | 广泛 | 有限 |

**我们选择两者**：
- 给用户自由选择
- 满足不同场景需求

### 2. 库 vs 可执行文件

```cmake
# 静态库
add_library(nano_redis STATIC ${SOURCES})

# 测试可执行文件
add_executable(version_test tests/version_test.cc)
target_link_libraries(version_test nano_redis GTest::gtest)
```

**好处**：
- 代码复用
- 独立测试
- 便于静态分析

## 错误处理模式

### Status 模式（类似 gRPC/Abseil）

```cpp
Status result = SomeOperation();
if (!result.ok()) {
  LOG(ERROR) << result.ToString();
  return result;
}
```

**vs 异常**：
```cpp
try {
  SomeOperation();
} catch (const std::exception& e) {
  LOG(ERROR) << e.what();
}
```

**我们选择 Status 因为**：
- 零开销（无异常处理机制）
- 明确的错误处理路径
- 与 Abseil 一致

## 下一步学习

在下一个提交中，我们将学习：
- Arena Allocator 的设计
- 内存分配的优化技巧
- 指针 bump 分配算法

## 推荐阅读

### C++ 基础
- [C++ Reference - constexpr](https://en.cppreference.com/w/cpp/language/constexpr)
- [C++ Reference - enum](https://en.cppreference.com/w/cpp/language/enum)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

### 构建系统
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
- [Bazel Tutorial](https://bazel.build/start/intro-build-run)

### 测试
- [GoogleTest Primer](https://google.github.io/googletest/primer.html)
