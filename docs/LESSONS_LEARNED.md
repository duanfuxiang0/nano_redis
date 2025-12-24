# Commit 1: Hello World - 学习要点

## C++17 特性

### enum class 强类型枚举

```cpp
enum class StatusCode : int {
  kOk = 0,
  kNotFound = 1,
  kInvalidArgument = 2,
  kInternalError = 3,
  kAlreadyExists = 4,
};

// 使用
StatusCode code = StatusCode::kOk;
```

**优点**：
- 类型安全（避免隐式转换）
- 明确的作用域（kOk 而不是 OK）

### constexpr 编译期常量

```cpp
constexpr int kMajorVersion = 0;
constexpr int kMinorVersion = 1;
constexpr int kPatchVersion = 0;
```

**优点**：
- 编译期计算，无运行时开销
- 可用于模板参数

### inline 函数

```cpp
inline const char* GetVersion() {
  return kVersionString;
}
```

**优点**：
- 头文件中定义，避免 ODR 问题
- 编译器可能内联优化

## 项目组织

### Apache License 头注释

```cpp
// Copyright 2025 Nano-Redis Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// ...
```

**意义**：
- 遵循开源协议规范
- 便于第三方使用

### 命名空间隔离

```cpp
namespace nano_redis {
  // 所有公共 API
}

// 使用
nano_redis::GetVersion();
```

### 包含保护宏

```cpp
#ifndef NANO_REDIS_VERSION_H_
#define NANO_REDIS_VERSION_H_
// ...
#endif
```

## 测试框架

### GoogleTest 断言

```cpp
// EXPECT_*: 失败继续执行
EXPECT_EQ(version, "0.1.0");
EXPECT_TRUE(condition);

// ASSERT_*: 失败立即停止
ASSERT_NE(version, nullptr);
```

### 测试命名规范

```
<Feature>Test, <TestName>
例如: VersionTest, GetVersionReturnsCorrectString
```

---

# Commit 2: Arena Allocator - 学习要点

## 内存分配器原理

### Bump Allocator

**原理**：最简单的分配策略，只向前移动指针。

```cpp
char* ptr_;  // 当前分配位置
char* end_;  // 当前块结束位置

void* Allocate(size_t size) {
  if (ptr_ + size > end_) {
    // 分配新的块
  }
  char* result = ptr_;
  ptr_ += size;
  return result;
}
```

**优点**：
- O(1) 分配速度
- 无需维护空闲链表

**缺点**：
- 不能单独释放对象
- 需要一次性释放整个 Arena

### Free List Allocator

**原理**：维护空闲块链表，支持单独释放。

**优点**：灵活的内存管理

**缺点**：
- 实现复杂
- 性能较低
- 有碎片

### Slab Allocator

**原理**：固定大小的块，无碎片。

**适用场景**：
- 频繁分配/释放相同大小的对象

## 内存对齐

```cpp
// 检查对齐
bool is_aligned = (ptr % alignment) == 0;

// 计算下一个对齐地址
uintptr_t next_aligned = (ptr + alignment - 1) & ~(alignment - 1);
```

**为什么需要对齐？**
- CPU 访问对齐的内存更快（SIMD 指令）
- 某些架构要求强制对齐（如 ARM）

## RAII 模式

```cpp
class ScopedArena {
 public:
  ScopedArena() : arena_() {}
  ~ScopedArena() { arena_.Reset(); }
  
  Arena* get() { return &arena_; }

 private:
  Arena arena_;
};

// 使用
{
  ScopedArena scoped_arena;
  void* ptr = scoped_arena.get()->Allocate(64);
  // 自动释放
}
```

---

# Commit 3: flat_hash_map vs unordered_map - 学习要点

## 哈希表基础

### Hash Function

```cpp
// Split hash into H1 and H2
size_t hash = std::hash<std::string>()(key);
size_t h1 = hash >> 7;        // 用于 bucket 选择
uint8_t h2 = hash & 0x7F;      // 用于 SIMD 匹配
```

### Collision Resolution

**Chaining**：
- 链表解决冲突
- 插入 O(1)，查找 O(1) 平均
- 内存不连续

**Open Addressing**：
- 探测下一个空槽
- Linear probing: i+1, i+2, i+3...
- Quadratic probing: i+1, i+4, i+9...
- Swiss Table: group probing

### Load Factor

```cpp
// 当元素数量 > 容量 * load_factor 时 rehash
if (size_ > capacity() * kMaxLoadFactor) {
  Rehash(capacity() * 2);
}
```

## SIMD 指令

### SSE/AVX

```cpp
// 加载 128 位（16 bytes）
__m128i data = _mm_loadu_si128((__m128i*)ptr);

// 设置向量（广播）
__m128i value = _mm_set1_epi8(target);

// 比较向量
__m128i cmp = _mm_cmpeq_epi8(data, value);

// 提取掩码
int mask = _mm_movemask_epi8(cmp);
```

### 位操作

```cpp
// 计算尾随零（最低位索引）
int idx = __builtin_ctz(mask);  // Count Trailing Zeros

// 清除最低位
mask &= mask - 1;
```

## Swiss Table 核心概念

### Control Bytes

```cpp
// 每个 bucket 对应一个控制字节
struct ControlGroup {
  uint8_t ctrl[16];  // 16 个控制字节
  
  // 特殊值:
  static constexpr uint8_t kEmpty = -128;   // 0x80: 空槽
  static constexpr uint8_t kDeleted = -2;   // 0xFE: 已删除
  static constexpr uint8_t kSentinel = -1;  // 0xFF: 哨兵位
  static constexpr uint8_t kMask = 0x7F;    // H2 hash 的掩码
};
```

### SIMD 探测流程

```cpp
// 1. 加载 16 个 control bytes
__m128i ctrl = _mm_loadu_si128((__m128i*)control_ptr);

// 2. 设置目标值（H2 hash）
__m128i target = _mm_set1_epi8(h2);

// 3. 比较向量
__m128i cmp = _mm_cmpeq_epi8(ctrl, target);

// 4. 提取匹配掩码
unsigned mask = _mm_movemask_epi8(cmp);

// 5. 遍历匹配的槽
while (mask) {
  int idx = __builtin_ctz(mask);  // 最低有效位
  if (CheckSlot(start_idx + idx, key)) {
    return &entries_[start_idx + idx];
  }
  mask &= mask - 1;  // 清除最低位
}
```

---

# Commit 4: 字符串处理 - std::string 高效使用

## SSO (Small String Optimization)

### 判断字符串是否使用 SSO

```cpp
constexpr size_t SSOThreshold() {
  std::string tmp;
  return tmp.capacity();  // 空字符串的 capacity 就是 SSO 阈值
}

bool IsSSO(const std::string& s) {
  return s.size() <= SSOThreshold();
}
```

### SSO 对性能的影响

- **小字符串**：零堆分配，栈上存储
- **缓存友好**：连续内存布局
- **自动优化**：编译器和库自动处理

### SSO 阈值

```
GCC/Clang 默认: 15 字节（不包含 '\0'）
MSVC 默认: 15 字节

为什么是 15？
  - 16 字节对齐（cache line 友好）
  - 包含 size 字段（1 字节）
  - 剩余 15 字节存储字符
```

## 字符串拷贝优化

### std::move 语义

```cpp
// 转移所有权，避免深拷贝
std::string src = "Hello";
std::string dest = std::move(src);  // src 变为空

// 适用场景：长字符串（>15 字节）
// 不适用场景：短字符串（SSO 模式）
```

### 返回值优化（RVO）

```cpp
// C++17 强制 RVO
std::string CreateString() {
  std::string s = "Hello";
  return s;  // 直接返回，无拷贝
}
```

### 引用传递

```cpp
// 只读：const 引用
void Process(const std::string& s);

// 写入：指针或引用
void Modify(std::string* s);
void Modify(std::string& s);
```

## 内存预分配

### reserve() 的作用

```cpp
std::string s;
s.reserve(1000);  // 预分配 1000 字节

for (int i = 0; i < 100; ++i) {
  s += "word";  // 无重新分配
}
```

### 预分配的好处

- 避免多次重新分配
- 减少内存拷贝
- 提升性能 2-5x

## 零拷贝操作

### 避免临时字符串

```cpp
// ❌ 创建临时字符串
result += "[" + item + "]";

// ✅ 直接追加
result += "[";
result += item;
result += "]";
```

### 使用 RVO/NRVO

```cpp
// ✅ 返回值优化
std::string Build() {
  std::string result;
  // ...
  return result;  // NRVO
}
```

## 字符串操作性能对比

### ToLower 原地 vs 拷贝

```cpp
// 原地修改：更快，无额外分配
void ToLower(std::string* s) {
  for (char& c : *s) {
    c = ToLowerChar(c);
  }
}

// 拷贝返回：保留原字符串
std::string ToLowerCopy(std::string s) {
  ToLower(&s);
  return s;  // RVO
}
```

### Split/Join 优化

```cpp
// ✅ 高效：预分配
std::vector<std::string> Split(const std::string& s, char delim) {
  std::vector<std::string> result;
  size_t count = std::count(s.begin(), s.end(), delim) + 1;
  result.reserve(count);  // 避免多次重新分配
  // ...
}

std::string Join(const std::vector<std::string>& parts,
                const std::string& delimiter) {
  // 计算最终大小
  size_t total_size = 0;
  for (const auto& part : parts) {
    total_size += part.size();
  }
  total_size += delimiter.size() * (parts.size() - 1);

  std::string result;
  result.reserve(total_size);  // 一次性分配
  // ...
}
```

## 性能优化总结

| 优化技术 | 提升倍数 | 适用场景 |
|---------|---------|----------|
| SSO（小字符串） | 20x | ≤15 字符 |
| std::move（长字符串） | 20x | >15 字符 |
| const& 引用传递 | ∞ | 只读参数 |
| reserve 预分配 | 2-5x | 已知大小 |
| Split/Join 预分配 | 2.5x | 分割/连接操作 |
| 避免临时字符串 | 5x | 循环拼接 |
| RVO/NRVO 返回值优化 | ∞ | 函数返回 |

## 最佳实践

### 1. 使用 const& 传递只读字符串

```cpp
// ✅ 推荐
bool IsValid(const std::string& s);

// ❌ 不推荐
bool IsValid(std::string s);
```

### 2. 使用 std::move 转移所有权

```cpp
// ✅ 推荐
std::string dest = std::move(src);

// ❌ 不推荐
std::string dest = src;  // 不必要的拷贝
```

### 3. 预分配已知大小的字符串

```cpp
// ✅ 推荐
std::string s;
s.reserve(expected_size);
for (...) {
  s += data;
}

// ❌ 不推荐
std::string s;
for (...) {
  s += data;  // 多次重新分配
}
```

### 4. 避免临时字符串

```cpp
// ✅ 推荐
result += "[";
result += item;
result += "]";

// ❌ 不推荐
result += "[" + item + "]";  // 创建临时字符串
```

### 5. 使用 RVO 返回字符串

```cpp
// ✅ 推荐
std::string Build() {
  std::string result;
  // ...
  return result;  // RVO
}

// ❌ 不推荐
void Build(std::string* result) {
  // ...
}
```

## 常见误区

### 误区 1：std::string 总是堆分配

**真相**：
- 小字符串（≤15 字节）使用 SSO，栈上分配
- 大字符串（>15 字节）才使用堆分配

### 误区 2：std::move 总是更快

**真相**：
- 短字符串（SSO）：move 和 copy 性能相同
- 长字符串（堆）：move 比 copy 快 20x

### 误区 3：const& 总是更好

**真相**：
- 只读参数：const& 更好
- 需要转移所有权：值传递 + std::move 更好

### 误区 4：reserve 总是值得

**真相**：
- 已知大小：reserve 值得
- 未知大小：reserve 可能浪费内存

## 性能调优建议

1. **优先使用 SSO**
   - 保持字符串短小（≤15 字节）
   - 避免不必要的字符串扩展

2. **减少字符串拷贝**
   - 使用 const& 传递只读字符串
   - 使用 std::move 转移所有权

3. **预分配内存**
   - 已知大小：使用 reserve()
   - 未知大小：估算后 reserve()

4. **避免临时字符串**
   - 直接追加而不是拼接
   - 使用 RVO 返回字符串

5. **选择合适的字符串操作**
   - 原地修改：ToLower(&s)
   - 保留原字符串：ToLowerCopy(s)
