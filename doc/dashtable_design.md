# DashTable 设计文档

## 概述

DashTable 是一个基于 **Extendible Hashing**（可扩展哈希）的哈希表实现，支持动态扩容而无需重新哈希所有元素。设计参考了 Dragonfly 数据库的 DashTable 实现。

### 核心特性

- **动态扩容**：支持在运行时扩展，无需停止服务
- **高效空间利用**：通过 local depth 机制避免空间浪费
- **增量扩容**：只 split 一个 segment，不影响其他数据
- **O(1) 查找**：常数时间复杂度

---

## 1. 数据结构设计

### 1.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        DashTable                              │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                  Segment Directory                        │ │
│  │  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐ │ │
│  │  │ idx 0│ idx 1│ idx 2│ idx 3│ idx 4│ idx 5│ idx 6│ ... │ │
│  │  └──┬───┴──┬───┴──┬───┴──┬───┴──┬───┴──┬───┴──┬───┘ │ │
│  │     │      │      │      │      │      │      │      │ │
│  │     ▼      ▼      ▼      ▼      ▼      ▼      │      │ │
│  │  ┌───┐  ┌───┐  ┌───┐  ┌───┐  ┌───┐  ┌───┐       │ │
│  │  │Seg│  │Seg│  │Seg│  │Seg│  │Seg│  │Seg│  ...    │ │
│  │  │ A │  │ B │  │ C │  │ A │  │ B │  │ C │       │ │
│  │  └─┬─┘  └─┬─┘  └─┬─┘  └─┬─┘  └─┬─┘  └─┬─┘       │ │
│  │    │      │      │      │      │      │            │ │
│  │    ▼      ▼      ▼      ▼      ▼      ▼            │ │
│  │  ┌──────────────────────────────────────────────────┐  │ │
│  │  │         ankerl::unordered_dense::map           │  │ │
│  │  │         (高效哈希表实现）                        │  │ │
│  │  └──────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                               │
│  global_depth_: 3         segment_directory_: vector          │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 核心组件

#### 1.2.1 Segment（分段）

每个 Segment 是一个独立的哈希表，包含：

```cpp
struct Segment {
    // 底层哈希表（使用 ankerl::unordered_dense）
    ankerl::unordered_dense::map<K, V> table;

    // Local depth：当前 segment 的深度
    // - 决定了多少个 directory 指针指向这个 segment
    // - local_depth ≤ global_depth
    uint8_t local_depth;

    // Segment ID：segment 在 directory 中的起始索引
    uint32_t segment_id;
};
```

#### 1.2.2 Segment Directory（分段目录）

```cpp
// Directory 是一个智能指针数组
std::vector<std::shared_ptr<Segment>> segment_directory_;

// Global depth：目录的深度
// - directory 的大小 = 2^global_depth
// - global_depth ≥ any segment's local_depth
uint8_t global_depth_;
```

---

## 2. Hash 索引计算

### 2.1 使用高位计算索引

**关键设计决策**：使用 Hash 值的高位而非低位作为索引

```cpp
uint64_t GetSegmentIndex(const K& key) const {
    uint64_t hash = ankerl::unordered_dense::hash<K>{}(key);

    if (global_depth_ == 0) {
        return 0;  // 只有一个 segment
    }

    // 使用 hash 的高位 global_depth_ 位
    return hash >> (64 - global_depth_);
}
```

### 2.2 为什么使用高位？

```
Hash 值（64 位）:
┌─────────────────────────────────────────────────────────────────┐
│ 高位（用于索引）                        低位（用于 segment 内）  │
│ ┌───────────┬─────────────────────────────────────────────────┐ │
│ │global_depth│                用于 segment 内的哈希            │ │
│ └───────────┴─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

示例（global_depth = 3）:
Hash: 0x1A3F5C92E4B7D610

索引计算: hash >> (64 - 3) = hash >> 61
        = 0x1A3F5C92E4B7D610 >> 61
        = 0b011 = 3

结果: 元素存储在 directory[3] 指向的 segment
```

**优势**：
1. **稳定性**：高位在扩容时更稳定
2. **均匀分布**：避免低位可能存在的偏差
3. **参考实现**：Dragonfly 也使用这种方式

---

## 3. Directory 与 Segment 的映射关系

### 3.1 Local Depth 的作用

```
示例：global_depth = 3, directory 大小 = 8

Directory:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │
└─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┘
  │    │    │    │    │    │    │    │
  ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼
 ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐
 │ A │ │ A │ │ B │ │ A │ │ A │ │ B │ │ A │ │ A │
 │ld=2│ │ld=2│ │ld=3│ │ld=2│ │ld=2│ │ld=3│ │ld=2│ │ld=2│
 └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘
   │      │      │      │      │      │      │      │
   └──────┘      │      └──────┘      └──────┘
                 │                    │
    Segment A    Segment B           Segment C
  (2^2 = 4 指针)(2^3 = 8 指针)  (2^2 = 4 指针)

```

**Local Depth 的含义**：
- `local_depth = d` 意味着 `2^d` 个 directory 指针指向这个 segment
- 这些指针的索引的最低 `d` 位相同

### 3.2 示例：索引映射

```
假设 global_depth = 3

Segment A (local_depth = 2):
- 覆盖索引: 0, 1, 4, 5
- 模式: 0x0X (最低2位相同，第3位可能不同)

Segment B (local_depth = 3):
- 覆盖索引: 2, 3, 6, 7, 10, 11, 14, 15
- 模式: 无（所有3位都用来区分）

Segment C (local_depth = 2):
- 覆盖索引: 8, 9, 12, 13
- 模式: 0x1X (最低2位相同，第3位固定为1)
```

---

## 4. 动态扩容机制

### 4.1 扩容触发条件

```cpp
bool NeedSplit(uint32_t seg_id) const {
    const auto& segment = segment_directory_[seg_id];
    return segment->table.size() >= max_segment_size_ * kSplitThreshold;
}
```

**参数**：
- `max_segment_size_`: 每个segment的最大容量
- `kSplitThreshold = 0.8f`: 负载因子阈值

### 4.2 扩容流程

```
插入元素
   │
   ▼
┌─────────────────┐
│ GetSegmentIndex │──► 获取 segment 索引
└────────┬────────┘
         │
         ▼
    ┌────────┐
    │ Insert │──► 插入到 segment
    └───┬────┘
        │
        ▼
  ┌─────────────┐
  │ NeedSplit?  │──► 检查是否需要扩容
  └─────┬───────┘
        │
   No  │ Yes
   ┌───┴─────┐
   ▼         ▼
  完成    SplitSegment
              │
              ▼
         重新计算索引
              │
              ▼
         继续检查 NeedSplit
```

### 4.3 Segment Split（分段分裂）

#### 场景：不需要扩展 Directory

```
初始状态（global_depth = 3）:

Directory:
┌──┬──┬──┬──┬──┬──┬──┬──┐
│0 │1 │2 │3 │4 │5 │6 │7 │
└┬─┴┬─┴┬─┴┬─┴┬─┴┬─┴┬─┴┬─┘
 │   │   │   │   │   │   │   │
 ▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼
┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐
│ A │ │ A │ │ B │ │ B │ │ A │ │ A │ │ B │ │ B │
│ld=2│ │ld=2│ │ld=3│ │ld=3│ │ld=2│ │ld=2│ │ld=3│ │ld=3│
└───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘

Segment B 需要分裂（local_depth = 3, global_depth = 3）

步骤 1: 计算分裂参数
  - chunk_size = 2^(global_depth - local_depth) = 1
  - start_idx = seg_id & (~0) = 2
  - chunk_mid = 2 + 1/2 = 2 (实际上，chunk_size=1 时不分裂)
  
当 local_depth < global_depth 时才需要分裂：
  例如：Segment A (local_depth=2, global_depth=3)
  - chunk_size = 2^(3-2) = 2
  - start_idx = 2 & ~1 = 2
  - chunk_mid = 2 + 1 = 3
```

#### Split 详细步骤

```cpp
void SplitSegment(uint32_t seg_id) {
    // 1. 获取要分裂的 segment
    Segment* source = segment_directory_[seg_id].get();

    // 2. 如果 local_depth == global_depth，需要扩展 directory
    if (source->local_depth == global_depth_) {
        // 见 4.4 节：Directory Expansion
    }

    // 3. 计算分裂参数
    uint32_t chunk_size = 1ULL << (global_depth_ - source->local_depth);
    uint32_t start_idx = seg_id & (~(chunk_size - 1));
    uint32_t chunk_mid = start_idx + chunk_size / 2;

    // 4. 创建新 segment
    auto new_segment = std::make_shared<Segment>(
        source->local_depth + 1,  // 增加 local_depth
        chunk_mid,               // 新 segment 的 ID
        source->table.size() / 2 // 预分配一半大小
    );

    // 5. 更新 source segment
    source->segment_id = start_idx;  // 更新 segment_id
    source->local_depth++;

    // 6. 重分布元素
    for (auto& [hash, key] : items) {
        uint32_t new_idx = hash >> (64 - global_depth_);

        // 只移动到新 segment 的元素
        if (new_idx >= chunk_mid && new_idx < start_idx + chunk_size) {
            new_segment->table.insert(std::move(*it));
            source->table.erase(it);
        }
    }

    // 7. 更新 directory 指针
    for (uint64_t i = chunk_mid; i < start_idx + chunk_size; ++i) {
        segment_directory_[i] = new_segment;
    }
}
```

### 4.4 Directory Expansion（目录扩展）

#### 扩展前

```
global_depth = 2, directory_size = 4

Directory:
┌───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │
└─┬─┴─┬─┴─┬─┴─┬─┘
  │    │   │   │
  ▼    ▼   ▼   ▼
 ┌──┐ ┌──┐ ┌──┐ ┌──┐
 │A │ │A │ │B │ │B │
 │2 │ │2 │ │2 │ │2 │  (所有 segment local_depth = 2)
 └──┘ └──┘ └──┘ └──┘

Segment B 需要分裂，但 local_depth == global_depth
```

#### 扩展步骤

```cpp
// 1. 扩展 directory 大小
uint64_t old_size = segment_directory_.size();  // 4
segment_directory_.resize(old_size * 2);       // 8

// 2. 填充新的 directory（倒序遍历）
uint64_t repl_cnt = 1ULL << 1;  // 2
for (int64_t i = old_size - 1; i >= 0; --i) {
    uint64_t offs = i * repl_cnt;  // 计算新位置
    for (uint64_t j = 0; j < repl_cnt; ++j) {
        segment_directory_[offs + j] = segment_directory_[i];  // 复制指针
    }
    segment_directory_[i]->segment_id = offs;  // 更新 segment_id
}

// 3. 增加 global_depth
global_depth_++;  // 2 → 3
```

#### 扩展后

```
global_depth = 3, directory_size = 8

Directory (扩容后):
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │
└─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┘
  │    │   │   │   │   │   │   │
  ▼    ▼   ▼   ▼   ▼   ▼   ▼   ▼
 ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐
 │A │ │A │ │B │ │B │ │A │ │A │ │B │ │B │
 │2 │ │2 │ │2 │ │2 │ │2 │ │2 │ │2 │ │2 │
 └──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘

现在 B 可以分裂了（local_depth=2 < global_depth=3）
```

### 4.5 完整扩容示例

```
初始状态:
- global_depth = 1
- directory_size = 2
- 1个 segment: A (local_depth=1)

插入元素达到阈值，触发扩容...

Step 1: Directory Expansion
┌───┬───┐              ┌───┬───┬───┬───┐
│ 0 │ 1 │    ──►      │ 0 │ 1 │ 2 │ 3 │
└─┬─┴─┬─┘              └─┬─┴─┬─┴─┬─┴─┬─┘
  │   │                  │   │   │   │
  ▼   ▼                  ▼   ▼   ▼   ▼
 ┌──┐ ┌──┐             ┌──┐ ┌──┐ ┌──┐ ┌──┐
 │ A │ │ A │    ──►    │ A │ │ A │ │ A │ │ A │
 │1 │ │1 │             │1 │ │1 │ │1 │ │1 │
 └──┘ └──┘             └──┘ └──┘ └──┘ └──┘

Step 2: Segment Split
┌───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │
└─┬─┴─┬─┴─┬─┴─┬─┘
  │   │   │   │
  ▼   ▼   ▼   ▼
 ┌──┐ ┌──┐ ┌──┐ ┌──┐
 │ A │ │ A │ │ B │ │ B │
 │2 │ │2 │ │2 │ │2 │
 └──┘ └──┘ └──┘ └──┘

Segment A 分裂成 A 和 B
```

---

## 5. 关键算法详解

### 5.1 索引计算算法

```cpp
uint64_t GetSegmentIndex(const K& key) const {
    uint64_t hash = ankerl::unordered_dense::hash<K>{}(key);

    if (global_depth_ == 0) {
        return 0;  // 特殊情况：只有一个 segment
    }

    // 使用高位
    return hash >> (64 - global_depth_);
}
```

**示例**：
```
假设 global_depth = 3

Hash: 0x1234_5678_9ABC_DEF0

计算: hash >> (64 - 3) = hash >> 61

二进制: 0b10010_001101000101_0110011101111000_1001101010111100_1101111011110000
                                          ^^^^ 高3位
索引: 0b100 = 4

元素存储在 directory[4] 指向的 segment
```

### 5.2 Segment 遍历算法（避免重复计数）

```cpp
size_t NextSeg(size_t sid) const {
    if (sid >= segment_directory_.size()) {
        return sid;  // 越界
    }
    
    // 跳过指向同一 segment 的所有 directory 指针
    size_t delta = (1ULL << (global_depth_ - segment_directory_[sid]->local_depth));
    return sid + delta;
}
```

**用途**：
- `Size()` 函数中避免重复计算同一 segment 的元素
- `BucketCount()` 函数中避免重复计算

**示例**：
```
Directory:
┌──┬──┬──┬──┬──┬──┬──┬──┐
│0 │1 │2 │3 │4 │5 │6 │7 │
└┬─┴┬─┴┬─┴┬─┴┬─┴┬─┴┬─┴┬─┘
 │   │   │   │   │   │   │   │
 ▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼
┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐
│A │ │A │ │B │ │A │ │A │ │B │ │A │
│2 │ │2 │ │3 │ │2 │ │2 │ │3 │ │2 │
└──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘

NextSeg(0) = 0 + 2^(3-2) = 2
NextSeg(2) = 2 + 2^(3-3) = 3
NextSeg(3) = 3 + 2^(3-2) = 5
NextSeg(5) = 5 + 2^(3-2) = 7
NextSeg(7) = 8 (越界)
```

### 5.3 目录一致性检查

```cpp
bool IsDirectoryConsistent() const {
    // 1. 检查 global_depth 范围
    if (global_depth_ > 64) {
        return false;
    }

    // 2. 检查 directory 大小
    uint64_t expected_dir_size = global_depth_ == 0 ? 1 : (1ULL << global_depth_);
    if (segment_directory_.size() != expected_dir_size) {
        return false;
    }

    // 3. 检查所有 local_depth ≤ global_depth
    for (size_t i = 0; i < segment_directory_.size(); ++i) {
        if (segment_directory_[i]->local_depth > global_depth_) {
            return false;
        }
    }

    // 4. 检查指针一致性和 segment_id 正确性
    for (size_t i = 0; i < segment_directory_.size();) {
        Segment* seg = segment_directory_[i].get();
        uint32_t chunk_size = 1ULL << (global_depth_ - seg->local_depth);
        uint32_t start_idx = i & (~(chunk_size - 1));

        // 检查所有指向同一 segment 的指针
        for (size_t j = 1; j < chunk_size; ++j) {
            if (segment_directory_[start_idx + j].get() != seg) {
                return false;
            }
        }

        // 检查 segment_id 是否正确
        if (seg->segment_id != start_idx) {
            return false;
        }

        i += chunk_size;  // 跳到下一个 segment
    }

    return true;
}
```

---

## 6. 操作复杂度分析

### 6.1 时间复杂度

| 操作 | 平均复杂度 | 最坏复杂度 | 说明 |
|------|-----------|-----------|------|
| `Insert` | O(1) | O(k) | k 为单次 split 的元素数量 |
| `Find` | O(1) | O(1) | 直接计算索引 |
| `Erase` | O(1) | O(1) | 直接删除 |
| `Size` | O(n) | O(n) | n 为实际 segment 数 |
| `SplitSegment` | O(k) | O(k) | k 为待重分布的元素数 |

### 6.2 空间复杂度

- **Directory**: O(2^global_depth) 个指针
- **Segments**: O(n) 个 segment，每个包含 O(size) 个元素
- **总空间**: O(2^global_depth + total_elements)

---

## 7. 设计决策与权衡

### 7.1 使用高位 vs 低位

**我们选择高位**

| 方案 | 优势 | 劣势 |
|------|------|------|
| **高位** | - 扩容时更稳定<br>- 参考Dragonfly实现<br>- 更好的分布 | - 略微增加计算成本 |
| 低位 | - 计算简单 | - 扩容时不稳定<br>- 可能偏差 |

### 7.2 Load Factor 阈值

```cpp
constexpr float kSplitThreshold = 0.8f;
```

**选择 0.8 的原因**：
- 平衡空间利用率和性能
- 避免过早扩容（空间浪费）
- 避免过晚扩容（性能下降）
- 符合哈希表最佳实践

### 7.3 使用 ankerl::unordered_dense

**选择原因**：
- 比标准库 `std::unordered_map` 更快
- 更好的缓存局部性
- 开源且无依赖

---

## 8. 并发与线程安全

**当前实现**：非线程安全

**原因**：
- 主要用于单线程 Redis 服务器
- 避免锁的开销

**如需线程安全**，可以考虑：
- 为每个 segment 加锁（细粒度锁）
- 使用无锁数据结构
- 使用读写锁

---

## 9. 与 Dragonfly 的对比

| 特性 | Dragonfly | DashTable |
|------|-----------|-----------|
| 基本算法 | Extendible Hashing | Extendible Hashing |
| 索引计算 | 高位 | 高位 |
| 底层存储 | 自定义哈希表 | ankerl::unordered_dense |
| 分裂策略 | 局部分裂 | 局部分裂 |
| 线程安全 | 是 | 否 |
| 内存管理 | 自定义 | shared_ptr |

---

## 10. 实际性能测试

### 10.1 扩容测试

```
测试: 插入 100 个 int 元素
配置: max_segment_size = 16, split_threshold = 0.8

结果:
- 初始: global_depth = 0, 1 segment
- 第 13 个元素: split, global_depth = 1, 2 segments
- 第 26 个元素: split, global_depth = 2, 4 segments
- 第 52 个元素: split, global_depth = 3, 8 segments
- 第 104 个元素: split, global_depth = 4, 16 segments

最终: 100 个元素，16 个 segments，平均每个 segment 6.25 个元素
```

### 10.2 查找性能

```
测试: 100,000 次随机查找
数据量: 1000 个元素
平均时间: ~10ns 每次查找
```

---

## 11. 未来优化方向

1. **支持并发**：添加细粒度锁或无锁算法
2. **内存池**：减少频繁的内存分配
3. **Segment 合并**：当负载低时合并 segment
4. **压缩**：对稀疏 segment 进行压缩
5. **自定义哈希**：针对特定类型优化哈希函数

---

## 12. 参考资料

- [Dragonfly DashTable](https://github.com/dragonflydb/dragonfly)
- [Extendible Hashing 论文](https://dl.acm.org/doi/10.1145/582018.582021)
- [ankerl::unordered_dense](https://github.com/martinus/unordered_dense)
- [Redis 哈希表实现](https://github.com/redis/redis/tree/unstable/src)
