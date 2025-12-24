# Commit 1: Hello World - 项目脚手架

## 概述

本提交建立了 Nano-Redis 项目的完整脚手架，包括：
- 项目结构
- 构建系统（Bazel + CMake）
- 基础头文件（version.h, status.h）
- 单元测试框架

## 代码统计

```
文件                      | 行数 | 说明
--------------------------|------|------
include/nano_redis/version.h |   30 | 版本号定义
include/nano_redis/status.h  |   75 | 错误处理
src/version.cc              |   12 | 版本实现
tests/version_test.cc        |   60 | 单元测试
--------------------------|------|------
总计                      |  177 |
```

## 设计决策

### 1. 为什么只选择 CMake？

| 构建系统 | 优点 | 缺点 | 使用场景 |
|---------|------|--------|---------|
| **CMake** | - 广泛支持<br/>- IDE 友好<br/>- 学习曲线低 | - 配置相对复杂 | 开发和生产环境 |

**决策**：使用 CMake，简化项目结构，降低学习成本。

### 2. 为什么分离头文件和实现？

```
include/nano_redis/     # 公共 API（稳定接口）
src/                  # 实现细节（内部可见）
tests/                # 测试代码
```

**优点**：
- 清晰的接口边界
- 便于库的使用者
- 减少编译依赖

### 3. Status 类的设计考虑

```cpp
enum class StatusCode : int {
  kOk = 0,
  kNotFound = 1,
  kInvalidArgument = 2,
  kInternalError = 3,
  kAlreadyExists = 4,
};
```

**为什么用 enum class？**
- 类型安全（避免隐式转换）
- 明确的作用域（kOk 而不是 OK）

**为什么不用 std::optional？**
- 需要传递错误信息（不仅仅是成功/失败）
- 后续扩展需要错误详情（message）
- 与 Abseil 的风格保持一致

## 架构图

```
nano_redis/
├── include/nano_redis/    ← 公共头文件
│   ├── version.h
│   └── status.h
├── src/                   ← 实现文件
│   └── version.cc
├── tests/                 ← 单元测试
│   └── version_test.cc
├── docs/                  ← 教学文档
│   ├── DESIGN.md
│   ├── ARCHITECTURE.md
│   ├── PERFORMANCE.md
│   └── LESSONS_LEARNED.md
├── CMakeLists.txt          ← CMake 构建
└── README.md              ← 项目说明
```

## 性能分析

本阶段主要是脚手架代码，无性能关键路径。

### 编译时间

```bash
# CMake 增量编译
cmake --build build  # ~0.5s
```

## 学习要点

### C++20 特性

本提交使用的 C++20 特性：

1. **enum class**: 强类型枚举
   ```cpp
   enum class StatusCode : int { ... };
   ```

2. **constexpr**: 编译期常量
   ```cpp
   constexpr int kMajorVersion = 0;
   ```

3. **inline functions**: 头文件中定义函数
   ```cpp
   inline const char* GetVersion() { return kVersionString; }
   ```

### 项目组织

1. **Apache License 头注释**
   - 遵循开源协议规范
   - 便于第三方使用

2. **命名空间隔离**
   ```cpp
   namespace nano_redis {
     // 所有公共 API
   }
   ```

3. **包含保护宏**
   ```cpp
   #ifndef NANO_REDIS_VERSION_H_
   #define NANO_REDIS_VERSION_H_
   // ...
   #endif
   ```

### 测试框架

1. **GoogleTest 断言**
   - `EXPECT_*`: 失败继续执行
   - `ASSERT_*`: 失败立即停止

2. **测试命名规范**
   ```
   <Feature>Test, <TestName>
   例如: VersionTest, GetVersionReturnsCorrectString
   ```

## 下一步

下一个提交将实现 **Arena Allocator**，这是高性能内存管理的基础。

## 验收标准

- [x] 项目结构建立完成
- [x] CMake 构建系统配置完成
- [x] Abseil 集成完成
- [x] 版本号模块实现
- [x] 错误处理模块实现
- [x] 单元测试通过
- [x] 文档完整

---

# Commit 2: Arena Allocator - 内存分配器设计

## 概述

本提交实现了 Arena Allocator（区域分配器），这是高性能内存管理的基础。Arena 通过批量分配和统一释放，避免了频繁的 malloc/free 调用。

## 代码统计

```
文件                      | 行数 | 说明
--------------------------|------|------
include/nano_redis/arena.h |   48 | Arena 定义
src/arena.cc               |   85 | Arena 实现
tests/arena_test.cc        |  165 | 单元测试
tests/arena_bench.cc       |  102 | 性能基准
--------------------------|------|------
总计                      |  400 |
```

## 设计决策

### 1. 为什么用 Arena 而不是直接 malloc？

| 方案 | 优点 | 缺点 | 使用场景 |
|------|------|------|----------|
| **Arena** | - O(1) 分配速度<br/>- 无碎片<br/>- 缓存友好<br/>- 批量释放 | - 需要预知生命周期<br/>- 不能单独释放对象 | 短生命周期、大量小对象 |
| **malloc/free** | - 灵活的内存管理<br/>- 可单独释放对象 | - 系统调用开销<br/>- 碎片化<br/>- 复杂度高 | 长生命周期、不确定大小 |

**决策**：使用 Arena，适用于服务器场景中大量短生命周期的对象。

### 2. 为什么选择指针 bump 分配？

```
指针 bump 分配：
  ptr_ → [已分配区域] [空闲区域] → end_
         ↑              ↑
       分配位置       可分配空间

每次分配只需移动指针：ptr_ += size
```

**优点**：
- O(1) 分配速度
- 无需维护空闲链表
- 无需搜索最佳匹配块
- 简单高效

**缺点**：
- 不能单独释放对象
- 需要一次性释放整个 Arena

### 3. 为什么不用 TLS (Thread Local Storage)？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **TLS Arena** | - 无锁<br/>- 高并发性能 | - 每线程内存占用<br/>- 教学复杂度高 |
| **单线程 Arena** | - 简单直观<br/>- 内存占用小 | - 需要加锁（多线程） |

**决策**：第一阶段使用单线程 Arena，后续可扩展为 Per-thread Arena。

### 4. 内存对齐处理

```cpp
void* Allocate(size_t size, size_t alignment) {
  // 计算对齐所需的填充
  size_t offset = reinterpret_cast<uintptr_t>(ptr_) % alignment;
  size_t padding = (offset == 0) ? 0 : alignment - offset;
  
  char* result = ptr_ + padding;
  ptr_ += size + padding;
  
  return result;
}
```

**为什么需要对齐？**
- CPU 访问对齐的内存更快（SIMD 指令）
- 某些架构要求强制对齐（如 ARM）
- std::max_align_t 保证最大对齐要求

## 架构图

```
Arena 内存布局：

Block 0 (4KB):
┌────────────────────────────────┐
│ [ptr_已分配][空闲] → end_      │
└────────────────────────────────┘

Block 1 (4KB):  ┐
                │ blocks_ 向量
Block 2 (4KB):  │
                │

当 Block 0 不够时：
1. 分配新的 Block
2. ptr_, end_ 指向新 Block
3. 旧 Block 保留（直到 Reset）

Reset():
  - 释放所有 Block
  - ptr_, end_ = nullptr
```

## 性能分析

### 理论性能

| 操作 | Arena | malloc | 提升 |
|------|-------|--------|------|
| 分配 | O(1) | O(n) ~ O(log n) | ∞ |
| 释放 | O(1) 批量 | O(1) 单个 | N/A |
| 碎片 | 0% | ~15% | - |

### 实际性能（预期）

```
操作                | Arena  | malloc | 提升
--------------------|--------|--------|------
分配 1000 小对象    | 0.01ms | 0.10ms | 10x
释放 (批量)         | 0.001ms| N/A    | ∞
内存碎片           | 0%      | ~15%   | -
```

## 学习要点

### 内存分配器原理

1. **Bump Allocator**
   - 最简单的分配策略
   - 只能向前移动指针
   - 适合已知生命周期的对象

2. **Free List Allocator**
   - 维护空闲块链表
   - 支持单独释放
   - 实现复杂，性能较低

3. **Slab Allocator**
   - 固定大小的块
   - 无碎片
   - 适合频繁分配/释放相同大小的对象

### 内存对齐

```cpp
// 检查对齐
bool is_aligned = (ptr % alignment) == 0;

// 计算下一个对齐地址
uintptr_t next_aligned = (ptr + alignment - 1) & ~(alignment - 1);
```

### RAII 模式

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

## 验收标准

- [x] Arena 类实现完成
- [x] 支持内存对齐
- [x] 支持移动语义
- [x] 单元测试通过（12/12）
- [x] 性能基准测试实现
- [x] 文档完整

---

# Commit 3: 数据结构选型 - flat_hash_map vs unordered_map

## 概述

本提交引入了基于 Swiss Table 的 `flat_hash_map` 作为 Redis 键值存储的基础数据结构，并与标准库的 `unordered_map` 进行性能对比。

## 代码统计

```
文件                            | 行数 | 说明
--------------------------------|------|------
include/nano_redis/string_store.h |   82 | 字符串存储定义
src/string_store.cc              |   95 | 字符串存储实现
tests/hash_table_bench.cc        |  168 | 性能基准测试
--------------------------------|------|------
总计                            |  345 |
```

## 设计决策

### 1. 为什么选择 flat_hash_map 而不是 unordered_map？

| 特性 | flat_hash_map (Swiss Table) | unordered_map (标准库) |
|------|---------------------------|----------------------|
| **冲突处理** | Open addressing | Chaining (链表) |
| **探测策略** | Group probing + SIMD | Linear probing |
| **内存布局** | 连续数组 | Bucket 链表 |
| **缓存局部性** | 优秀（连续存储） | 差（链表分散） |
| **内存占用** | 较低（无链表指针） | 较高（额外指针） |
| **删除复杂度** | 需标记删除 | 直接释放 |
| **迭代器稳定性** | rehash 时失效 | 稳定 |

**决策**：使用 flat_hash_map，优势在于：
1. Cache 友好（连续内存）
2. SIMD 加速（一次探测 16 个槽）
3. 内存效率高（无链表指针）
4. 适用于读多写少的场景

### 2. Open Addressing vs Chaining

```
Open Addressing (flat_hash_map):
  ┌─────────────────────────────────┐
  │ [Entry 0] [Entry 1] [Entry 2] ... │
  └─────────────────────────────────┘
  冲突时：在数组中查找下一个空槽

Chaining (unordered_map):
  ┌─────────────────────────────────┐
  │ bucket[0] → node → node → null  │
  │ bucket[1] → node → null        │
  └─────────────────────────────────┘
  冲突时：添加到链表
```

### 3. Swiss Table 的核心设计

#### Control Bytes

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

#### SIMD 探测

```cpp
// 使用 SSE 指令一次比较 16 个槽
__m128i ctrl = _mm_loadu_si128((__m128i*)control_ptr);
__m128i target = _mm_set1_epi8(h2);  // H2 hash
__m128i cmp = _mm_cmpeq_epi8(ctrl, target);

// 提取匹配位
unsigned mask = _mm_movemask_epi8(cmp);
while (mask) {
  int idx = __builtin_ctz(mask);  // 最低有效位
  if (CheckSlot(start_idx + idx, key)) {
    return &entries_[start_idx + idx];
  }
  mask &= mask - 1;  // 清除最低位
}
```

### 4. 为什么不使用 node_hash_map？

`node_hash_map` 虽然提供了指针稳定性，但存在：
- 双重间接访问（bucket → node → data）
- 额外的内存开销（每个节点单独分配）
- 缓存局部性差

**Redis 场景分析**：
- 命令通常不保留迭代器引用
- Key/Value 生命周期短（单次请求）
- 读多写少，查找性能优先

## 架构图

```
flat_hash_map 内存布局：

┌────────────────────────────────────────────────────┐
│ Control Bytes │ Metadata │  Entry 0  │  Entry 1 │
│ [16 bytes]    │ [hashes] │ [key,val] │ [key,val]│
└────────────────────────────────────────────────────┘
  Group 0 (16 bytes)

查找流程：
1. hash(key) = (H1, H2)
2. bucket = H1 % table_size
3. group_start = (bucket / 16) * 16
4. 使用 SIMD 比较 16 个 control bytes
5. 找到匹配的 H2 值
6. 完整比较 key
```

## 性能分析

### 理论性能

| 操作 | flat_hash_map | unordered_map | 复杂度 |
|------|--------------|---------------|--------|
| 插入 | O(1) 平均 | O(1) 平均 | 相同 |
| 查找 | O(1) 平均 | O(1) 平均 | 相同 |
| 删除 | O(1) 平均 | O(1) 平均 | 相同 |
| 遍历 | O(n) | O(n) | 相同 |

### 实际性能（预期）

```
基准测试（100K items, key=value=8 bytes）：

操作                | flat_hash_map | unordered_map | 提升
--------------------|--------------|---------------|------
Insert              | 120ms        | 280ms         | 2.3x
Lookup (cache hit)  | 80ms         | 150ms         | 1.9x
Delete              | 150ms        | 200ms         | 1.3x
Iterate             | 10ms         | 12ms          | 1.2x
Memory Usage        | 64MB         | 96MB          | 1.5x
```

## 学习要点

### 哈希表基础

1. **Hash Function**
   ```cpp
   // Split hash into H1 and H2
   size_t hash = std::hash<std::string>()(key);
   size_t h1 = hash >> 7;        // 用于 bucket 选择
   uint8_t h2 = hash & 0x7F;      // 用于 SIMD 匹配
   ```

2. **Collision Resolution**
   - **Chaining**: 链表解决冲突
   - **Open Addressing**: 探测下一个空槽
     - Linear probing: i+1, i+2, i+3...
     - Quadratic probing: i+1, i+4, i+9...
     - Swiss Table: group probing

3. **Load Factor**
   ```cpp
   // 当元素数量 > 容量 * load_factor 时 rehash
   if (size_ > capacity() * kMaxLoadFactor) {
     Rehash(capacity() * 2);
   }
   ```

### SIMD 指令

1. **SSE/AVX**
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

2. **位操作**
   ```cpp
   // 计算尾随零（最低位索引）
   int idx = __builtin_ctz(mask);  // Count Trailing Zeros
   
   // 清除最低位
   mask &= mask - 1;
   ```

## 验收标准

- [x] StringStore 类实现完成
- [x] StdStringStore 对比类实现完成
- [x] 性能基准测试实现
- [x] 文档完整
- [ ] 运行基准测试并验证性能提升

## 下一步

下一个提交将实现 **std::string 高效使用**，学习 SSO (Small String Optimization) 和字符串优化技巧。
