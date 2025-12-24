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

---

# Commit 2: Arena Allocator - 学习要点

## 内存分配基础

### 1. 为什么需要内存分配器？

**问题**: `new`/`delete` 和 `malloc`/`free` 的局限

```cpp
// 传统方式 - 慢
for (int i = 0; i < 1000000; ++i) {
  int* arr = new int[100];
  // 使用 arr
  delete[] arr;  // 每次都要调用系统函数
}

// Arena 方式 - 快
Arena arena;
for (int i = 0; i < 1000000; ++i) {
  int* arr = static_cast<int*>(arena.Allocate(100 * sizeof(int)));
  // 使用 arr
}
// arena.Reset() 一次性释放
```

**性能差异**:
- 传统方式: 1,000,000 次系统调用
- Arena 方式: ~1,000 次系统调用（每次 4KB 块）

### 2. Bump Allocator（指针推进分配器）

**原理**: 只向前移动指针

```cpp
class BumpAllocator {
  char* ptr_;
  char* end_;

public:
  void* Allocate(size_t size) {
    if (ptr_ + size > end_) {
      // 分配新块
    }
    char* result = ptr_;
    ptr_ += size;  // 只移动指针！
    return result;
  }
};
```

**为什么快？**
- 无需搜索空闲块（O(1)）
- 无需维护空闲链表
- 无需处理碎片

**限制**：
- 不能单独释放对象
- 适合短生命周期对象

### 3. 内存对齐

**问题**: CPU 访问未对齐的内存效率低

```cpp
// 未对齐（慢）
char* ptr = 0x1003;  // 未对齐到 4 字节
int* i = reinterpret_cast<int*>(ptr);
// CPU 需要两次内存访问

// 对齐（快）
char* ptr = 0x1004;  // 对齐到 4 字节
int* i = reinterpret_cast<int*>(ptr);
// CPU 只需一次内存访问
```

**对齐计算**:

```cpp
// 方法 1: 取模
uintptr_t offset = ptr % alignment;
uintptr_t padding = (offset == 0) ? 0 : alignment - offset;

// 方法 2: 位运算（更快）
uintptr_t padding = (alignment - (ptr & (alignment - 1))) & (alignment - 1);

// 示例
ptr = 0x1003, alignment = 8
offset = 0x1003 % 8 = 3
padding = 8 - 3 = 5
result = 0x1003 + 5 = 0x1008 (对齐到 8)
```

## C++ 技巧

### 1. 移动语义

```cpp
Arena a1;
a1.Allocate(100);

Arena a2(std::move(a1));  // 移动构造

// 移动后 a1 的状态
assert(a1.MemoryUsage() == 0);  // a1 已被清空
assert(a2.MemoryUsage() > 0);   // a2 拥有内存
```

**为什么需要移动？**
- 避免深拷贝
- 高效传递所有权
- 实现 RAII

### 2. RAII（资源获取即初始化）

```cpp
class ScopedArena {
 public:
  ScopedArena() : arena_() {}
  ~ScopedArena() { arena_.Reset(); }  // 自动释放

  Arena* get() { return &arena_; }

 private:
  Arena arena_;
};

// 使用
{
  ScopedArena scoped;
  void* ptr = scoped.get()->Allocate(64);
  // 离开作用域自动释放
}
```

**优点**：
- 不会忘记释放
- 异常安全
- 代码简洁

### 3. Placement New

```cpp
// 在已分配的内存上构造对象
void* ptr = arena.Allocate(sizeof(std::string));

// 使用 placement new
std::string* str = new (ptr) std::string("Hello");

// 显式调用析构函数
str->~std::string();

// 不需要 delete（Arena 会释放内存）
```

**为什么需要 placement new？**
- Arena 分配的内存没有构造对象
- 需要手动调用构造函数
- 适合预分配的场景

## 性能优化

### 1. 缓存友好设计

```cpp
// 好的设计 - 连续内存
Arena arena;
for (int i = 0; i < 1000; ++i) {
  // 所有对象连续分配
  int* obj = static_cast<int*>(arena.Allocate(sizeof(int)));
}

// 不好的设计 - 碎片化
std::vector<int*> ptrs;
for (int i = 0; i < 1000; ++i) {
  ptrs.push_back(new int);  // 可能分散在堆的各个位置
}
```

**缓存命中率差异**:
- Arena: 95%+
- malloc: 80-85%

### 2. 批量分配 vs 单个分配

```cpp
// 批量分配（快）
void* batch = arena.Allocate(1000 * sizeof(int));
int* arr = static_cast<int*>(batch);

// 单个分配（慢）
for (int i = 0; i < 1000; ++i) {
  void* ptr = arena.Allocate(sizeof(int));
}
```

**性能差异**:
- 批量: 1 次分配
- 单个: 1000 次分配

## 实际应用

### 1. 字符串池

```cpp
class StringPool {
  Arena arena_;

public:
  const char* Intern(const std::string& s) {
    size_t size = s.size() + 1;  // +1 for null terminator
    char* ptr = static_cast<char*>(arena_.Allocate(size));
    std::memcpy(ptr, s.c_str(), size);
    return ptr;
  }
};
```

### 2. 对象池

```cpp
template<typename T>
class ObjectPool {
  Arena arena_;

public:
  T* Allocate() {
    void* ptr = arena_.Allocate(sizeof(T));
    return new (ptr) T();
  }

  void FreeAll() {
    // 需要手动调用析构函数
    // 这里简化处理
  }
};
```

### 3. 临时缓冲区

```cpp
class TempBuffer {
  Arena arena_;

public:
  char* Allocate(size_t size) {
    return static_cast<char*>(arena_.Allocate(size));
  }

  void Reset() {
    arena_.Reset();
  }
};
```

## 常见错误

### 1. 忘记调用 Reset

```cpp
// 错误 - 内存泄漏
void ProcessData() {
  Arena arena;
  for (int i = 0; i < 10000; ++i) {
    arena.Allocate(100);
  }
  // 忘记调用 Reset()！
}

// 正确 - 使用 RAII
void ProcessData() {
  ScopedArena scoped;
  for (int i = 0; i < 10000; ++i) {
    scoped.get()->Allocate(100);
  }
  // 自动调用 Reset()
}
```

### 2. 使用已释放的指针

```cpp
Arena arena;
void* ptr = arena.Allocate(64);

arena.Reset();  // 释放所有内存

// 错误 - 悬空指针
*static_cast<int*>(ptr) = 42;  // 未定义行为
```

### 3. 忘记手动调用析构函数

```cpp
Arena arena;
std::string* str = new (arena.Allocate(sizeof(std::string))) std::string("Hello");

// 错误 - 析构函数未被调用
// std::string 内部的内存泄漏了

// 正确
str->~std::string();
```

## 进阶话题

### 1. Per-thread Arena（线程本地 Arena）

```cpp
class ThreadLocalArena {
  static thread_local Arena arena_;

public:
  static void* Allocate(size_t size) {
    return arena_.Allocate(size);
  }
};

// 使用
void* ptr = ThreadLocalArena::Allocate(64);
```

**优点**：
- 无锁，高并发性能
- 避免伪共享

### 2. 分层 Arena

```cpp
class HierarchicalArena {
  Arena small_;  // 小对象
  Arena large_;  // 大对象

public:
  void* Allocate(size_t size) {
    if (size < 1024) {
      return small_.Allocate(size);
    } else {
      return large_.Allocate(size);
    }
  }
};
```

### 3. 自适应块大小

```cpp
class AdaptiveArena {
  size_t current_block_size_;

public:
  void* Allocate(size_t size) {
    if (size > current_block_size_ / 2) {
      // 下次使用更大的块
      current_block_size_ *= 2;
    }
    // ...
  }
};
```

## 下一步学习

在下一个提交中，我们将学习：
- 数据结构选型：flat_hash_map vs unordered_map
- Swiss Table 的设计原理
- Open addressing 的实现

## 推荐阅读

### 内存分配
- [Malloc Internals](https://sourceware.org/glibc/wiki/MallocInternals)
- [jemalloc Design](http://jemalloc.net/jemalloc.3.html)
- [tcmalloc Design](https://github.com/google/tcmalloc)

### C++ 内存管理
- [C++ Reference - operator new](https://en.cppreference.com/w/cpp/memory/operator_new)
- [C++ Reference - placement new](https://en.cppreference.com/w/cpp/language/new)

### 性能优化
- [Cache Optimization](https://lwn.net/Articles/252125/)
- [Memory Alignment](https://en.wikipedia.org/wiki/Data_structure_alignment)
