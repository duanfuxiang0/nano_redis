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

---

# Commit 3: flat_hash_map vs unordered_map - 学习要点

## 哈希表基础

### 1. 哈希表的工作原理

**核心思想**: 通过 hash 函数将 key 映射到索引

```cpp
// 1. 计算 hash
size_t hash = std::hash<std::string>()("hello");

// 2. 计算索引
size_t index = hash % table_size;

// 3. 访问元素
value = table[index];
```

### 2. 冲突解决策略

**问题**: 两个不同的 key 可能映射到同一个索引

```cpp
std::hash<std::string>()("hello") % 16 = 5
std::hash<std::string>() world") % 16 = 5  // 冲突！
```

**解决方案**:

#### A. Chaining（链表法）

```cpp
// unordered_map 使用 chaining
struct Node {
  std::string key;
  std::string value;
  Node* next;  // 链表指针
};

std::vector<Node*> table;  // 每个 bucket 是一个链表

// 插入
size_t index = hash(key) % table_size;
Node* new_node = new Node(key, value, table[index]);
table[index] = new_node;  // 插入到链表头部
```

**优点**:
- 实现简单
- 删除容易
- 哈希函数选择宽松

**缺点**:
- 缓存不友好（链表跳转）
- 额外指针开销
- 内存碎片

#### B. Open Addressing（开放寻址）

```cpp
// flat_hash_map 使用 open addressing
struct Entry {
  std::string key;
  std::string value;
};

std::vector<Entry> table;  // 连续存储

// 插入 - 探测下一个空槽
size_t index = hash(key) % table_size;
while (table[index].occupied) {
  index = (index + 1) % table_size;  // Linear probing
}
table[index] = Entry(key, value);
```

**优点**:
- 缓存友好（连续内存）
- 无额外指针开销
- 内存紧凑

**缺点**:
- 删除复杂（需要标记）
- 聚集问题（clustering）
- 需要控制负载因子

### 3. Swiss Table 的 Group Probing

**传统 Linear Probing**:
```cpp
size_t index = hash(key) % table_size;
while (!table[index].empty) {
  index = (index + 1) % table_size;  // 逐个检查
}
```

**Swiss Table Group Probing**:
```cpp
// 16 个槽为一组
size_t group_start = (index / 16) * 16;

// 使用 SIMD 一次检查 16 个槽
__m128i ctrl = _mm_loadu_si128((__m128i*)&control[group_start]);
__m128i h2_vec = _mm_set1_epi8(h2);
__m128i match = _mm_cmpeq_epi8(ctrl, h2_vec);

// 提取匹配位
uint32_t mask = _mm_movemask_epi8(match);
```

**优势**:
- 一次指令检查 16 个槽
- 减少循环次数
- 更好的分支预测

## Swiss Table 设计

### 1. Split Hash（Hash 分割）

```cpp
// 将 hash 分成两部分
size_t hash = std::hash<std::string>()(key);

// H1: 高位，用于 bucket 选择
size_t h1 = hash >> 7;

// H2: 低 7 位，用于 SIMD 匹配
uint8_t h2 = hash & 0x7F;

// 为什么这样设计？
// - H1 有足够的随机性
// - H2 适合 SIMD 比较（7 bits）
```

### 2. Control Byte（控制字节）

```cpp
// 每个 entry 对应一个 control byte
struct Control {
  uint8_t ctrl[16];  // 一组 16 个控制字节
  
  // 特殊值：
  // 0x80 (128): 空槽
  // 0xFE (254): 已删除
  // 0xFF (255): 哨兵（探测结束）
  // 0x00-0x7F: 元素的 H2 hash
};

// 使用 MSB (最高位) 区分特殊值
// 0x80 = 0b10000000 (MSB=1, 空槽)
// 0xFE = 0b11111110 (MSB=1, 已删除)
// 0xFF = 0b11111111 (MSB=1, 哨兵)
// 0x5F = 0b01011111 (MSB=0, H2=0x5F)
```

### 3. SIMD 探测流程

```cpp
// 1. 定位 control group
size_t h1 = hash(key) >> 7;
size_t h2 = hash(key) & 0x7F;
size_t index = h1 % capacity;
size_t group_start = (index / 16) * 16;

// 2. 加载 control bytes
__m128i ctrl = _mm_loadu_si128((__m128i*)&control[group_start]);

// 3. 检查空槽（kEmpty=0x80）
__m128i empty = _mm_set1_epi8(kEmpty);
__m128i empty_match = _mm_cmpeq_epi8(ctrl, empty);
uint32_t empty_mask = _mm_movemask_epi8(empty_match);

// 4. 检查 H2 匹配
__m128i h2_vec = _mm_set1_epi8(h2);
__m128i h2_match = _mm_cmpeq_epi8(ctrl, h2_vec);
uint32_t h2_mask = _mm_movemask_epi8(h2_match);

// 5. 遍历匹配
uint32_t mask = empty_mask | h2_match;
while (mask) {
  int offset = __builtin_ctz(mask);  // 最低位索引
  int entry_idx = group_start + offset;
  
  if (control[entry_idx] == h2) {
    // 完整比较 key
    if (entries[entry_idx].key == key) {
      return &entries[entry_idx].value;
    }
  } else if (control[entry_idx] == kEmpty) {
    return nullptr;  // 未找到
  }
  
  mask &= mask - 1;  // 清除最低位
}
```

## C++ 技巧

### 1. 类型别名简化

```cpp
// 好的做法
using StringMap = absl::flat_hash_map<std::string, std::string>;

class StringStore {
  StringMap store_;  // 简洁明了
};

// 不好的做法
class StringStore {
  absl::flat_hash_map<std::string, std::string> store_;  // 太长
};
```

### 2. 移动语义优化

```cpp
// 拷贝版本（慢）
bool Put(const std::string& key, const std::string& value) {
  store_[key] = value;  // 两次拷贝（key 和 value）
}

// 移动版本（快）
bool Put(std::string&& key, std::string&& value) {
  store_[std::move(key)] = std::move(value);  // 零拷贝
}

// 使用
store.Put("key", "value");  // 使用拷贝版本
store.Put(std::string("key"), std::string("value"));  // 可以优化为移动
```

### 3. 引用返回

```cpp
// 返回引用 - 允许修改
std::string* GetMutable(const std::string& key) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    return &it->second;  // 返回指针，允许修改
  }
  return nullptr;
}

// 使用
if (std::string* value = store.GetMutable("key")) {
  *value = "new value";  // 直接修改
}

// 只读版本
bool Get(const std::string& key, std::string* value) const {
  auto it = store_.find(key);
  if (it != store_.end()) {
    if (value != nullptr) {
      *value = it->second;  // 拷贝到输出参数
    }
    return true;
  }
  return false;
}
```

### 4. 完美转发

```cpp
// 通用引用 + 完美转发
template<typename K, typename V>
bool Put(K&& key, V&& value) {
  store_[std::forward<K>(key)] = std::forward<V>(value);
  return true;
}

// 可以接受左值或右值
std::string k = "key";
store.Put(k, "value");              // 左值
store.Put("key", "value");          // 右值（临时对象）
store.Put(std::move(k), "value");   // 移动
```

## 性能优化

### 1. 预分配容量

```cpp
// 避免 rehash
class StringStore {
 public:
  void Reserve(size_t capacity) {
    store_.reserve(capacity);  // 预分配
  }
};

// 使用
StringStore store;
store.Reserve(100000);  // 预分配 100K 个槽

for (int i = 0; i < 100000; ++i) {
  store.Put("key_" + std::to_string(i), "value");
}
// 不会触发 rehash
```

**性能影响**:
- 无预分配: 120ms (包含多次 rehash)
- 预分配: 80ms (无 rehash)
- 提升: 1.5x

### 2. 选择合适的 hash function

```cpp
// 使用 Abseil 提供的 Hash
using StringMap = absl::flat_hash_map<
  std::string,
  std::string,
  absl::Hash<std::string>  // 高效的 hash 函数
>;

// vs 标准库 hash
// Abseil Hash 通常：
// - 更快（更好的算法）
// - 分布更均匀
// - 更少的冲突
```

### 3. 避免不必要的字符串拷贝

```cpp
// 差的做法
std::string key = "user_" + std::to_string(user_id);
std::string value = Serialize(user_data);
store.Put(key, value);  // 两次额外拷贝

// 好的做法
std::string key = "user_" + std::to_string(user_id);
std::string value = Serialize(user_data);
store.Put(std::move(key), std::move(value));  // 零拷贝

// 或者直接构造
store.Put("user_" + std::to_string(user_id), Serialize(user_data));
// 编译器可能优化（NRVO）
```

### 4. 批量操作

```cpp
// 逐个插入（慢）
for (const auto& [key, value] : items) {
  store.Put(key, value);
}

// 批量插入（快）
class StringStore {
 public:
  void PutBatch(const std::vector<std::pair<std::string, std::string>>& items) {
    store_.reserve(store_.size() + items.size());  // 预分配
    for (const auto& [key, value] : items) {
      store_[key] = value;
    }
  }
};

store.PutBatch(items);  // 单次预分配
```

## SIMD 指令

### 1. SSE 基础

```cpp
// 加载 128 位（16 bytes）
__m128i data = _mm_loadu_si128((__m128i*)ptr);

// 广播值（复制到所有元素）
__m128i value = _mm_set1_epi8(0x42);

// 比较向量
__m128i result = _mm_cmpeq_epi8(data, value);

// 提取掩码
int mask = _mm_movemask_epi8(result);
```

### 2. 位操作技巧

```cpp
// 计算尾随零（最低位 1 的位置）
unsigned mask = 0b00001000;
int idx = __builtin_ctz(mask);  // idx = 3

// 计算前导零（最高位 1 的位置）
unsigned mask = 0b00100000;
int idx = __builtin_clz(mask);  // idx = 26 (32-6)

// 清除最低位
mask &= mask - 1;

// 获取最低位
mask & -mask;  // 或 (mask & (~mask + 1))
```

### 3. SIMD 的优势

```cpp
// 传统方式 - 逐个比较
for (int i = 0; i < 16; ++i) {
  if (data[i] == target) {
    // ...
  }
}
// 16 次比较 + 16 次分支

// SIMD 方式 - 一次比较
__m128i cmp = _mm_cmpeq_epi8(data, target);
uint32_t mask = _mm_movemask_epi8(cmp);
while (mask) {
  int i = __builtin_ctz(mask);
  // ...
  mask &= mask - 1;
}
// 1 次向量比较 + 平均 log(16) 次迭代
```

## 常见错误

### 1. 忘记预分配

```cpp
// 错误 - 频繁 rehash
for (int i = 0; i < 1000000; ++i) {
  map[std::to_string(i)] = "value";
}

// 正确 - 预分配
map.reserve(1000000);
for (int i = 0; i < 1000000; ++i) {
  map[std::to_string(i)] = "value";
}
```

### 2. 频繁删除

```cpp
// 问题 - 删除标记堆积
for (int i = 0; i < 100000; ++i) {
  map.erase("key_" + std::to_string(i));
}
// 可能触发 rehash 清理删除标记

// 解决 - 批量清理
if (map.size() * 4 < map.capacity()) {
  map.shrink_to_fit();  // 清理删除标记
}
```

### 3. 遍历时修改

```cpp
// 危险 - 可能导致未定义行为
for (auto& [key, value] : map) {
  if (ShouldDelete(key)) {
    map.erase(key);  // 迭代器失效！
  }
}

// 正确 - 使用 erase_if（C++20）
std::erase_if(map, [](const auto& pair) {
  return ShouldDelete(pair.first);
});

// 或者记录后删除
std::vector<std::string> to_delete;
for (const auto& [key, _] : map) {
  if (ShouldDelete(key)) {
    to_delete.push_back(key);
  }
}
for (const auto& key : to_delete) {
  map.erase(key);
}
```

### 4. 误用 [] 操作符

```cpp
// 问题 - 总是插入元素
auto& value = map["non_existent_key"];
// 如果 key 不存在，会插入默认值（空字符串）

// 正确 - 使用 find
auto it = map.find("non_existent_key");
if (it != map.end()) {
  const auto& value = it->second;
}
```

## 进阶话题

### 1. 异构查找（Heterogeneous Lookup）

```cpp
// 问题 - 需要临时构造 string
std::string_view key_view = "hello";
auto it = map.find(std::string(key_view));  // 构造临时 string

// 解决 - 使用异构查找
template<typename K>
auto find(const K& key) const {
  return map.find(key);  // K 可以是 string_view
}

// 需要自定义 hash 和 equal_to
struct StringViewHash {
  size_t operator()(std::string_view sv) const {
    return std::hash<std::string_view>()(sv);
  }
};

using StringMap = absl::flat_hash_map<
  std::string,
  std::string,
  StringViewHash
>;
```

### 2. 自定义分配器

```cpp
// 集成 Arena
class StringStore {
  Arena arena_;
  
  using StringMap = absl::flat_hash_map<
    std::string,
    std::string,
    absl::Hash<std::string>,
    std::equal_to<std::string>,
    ArenaAllocator<std::pair<const std::string, std::string>>
  >;
  
  StringMap store_;
};
```

### 3. 并发哈希表

```cpp
// 简单的并发哈希表（使用读写锁）
class ConcurrentStringStore {
  mutable std::shared_mutex mutex_;
  StringMap store_;
  
 public:
  std::string Get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return store_[key];
  }
  
  void Put(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    store_[key] = value;
  }
};
```

## 下一步学习

在下一个提交中，我们将学习：
- std::string 的内部实现（SSO）
- 字符串优化技巧
- 避免不必要的字符串拷贝

## 推荐阅读

### 哈希表
- [Swiss Tables](https://abseil.io/about/design/swiss)
- [Hash Function Design](https://aras-p.info/blog/2016/08/02/Hash-Functions-Part-1/)
- [Understanding Linear Probing](https://lwn.net/Articles/620844/)

### SIMD
- [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/)
- [SIMD for Fun and Profit](https://www.agner.org/optimize/optimizing_cpp.pdf)

### C++ 技巧
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Effective Modern C++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
