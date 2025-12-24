# Commit 3: Swiss Table (flat_hash_map) 设计决策

## 为什么使用 flat_hash_map 而不是 unordered_map?

### 问题

标准库的 `std::unordered_map` 虽然提供了哈希表功能，但在性能上存在以下问题：

1. **链表冲突处理**：每个 bucket 维护一个链表，内存不连续
2. **额外指针开销**：每个节点需要额外的 next 指针
3. **缓存局部性差**：链表遍历导致频繁的 cache miss
4. **内存占用高**：每个元素需要额外的堆分配

### 解决方案：Abseil Swiss Table

Swiss Table 是 Google 开源的高效哈希表实现，使用 **open addressing**（开放寻址）+ **group probing**（分组探测）策略。

#### 核心设计

```
┌──────────────────────────────────────────────────────────────┐
│                    Swiss Table 内存布局                        │
├──────────────────────────────────────────────────────────────┤
│  Control Bytes  │  Metadata  │  Entry 0  │  Entry 1  │ ...   │
│  [16 bytes]     │  [hashes]  │  [key,val]│  [key,val]│       │
└──────────────────────────────────────────────────────────────┘

Control Bytes 结构:
  - 0x80 (-128): 空槽 (kEmpty)
  - 0xFE (-2):  已删除 (kDeleted)
  - 0xFF (-1):  哨兵位 (kSentinel)
  - 其他值:    元素的 H2 hash (7位)
```

#### SIMD 加速探测

```cpp
// 使用 SSE/AVX 指令一次比较 16 个槽
__m128i ctrl = _mm_loadu_si128((__m128i*)control_ptr);
__m128i target = _mm_set1_epi8(h2);
__m128i cmp = _mm_cmpeq_epi8(ctrl, target);

// 提取匹配位
unsigned mask = _mm_movemask_epi8(cmp);
while (mask) {
  int idx = __builtin_ctz(mask);
  if (CheckSlot(start_idx + idx, key)) {
    return &entries_[start_idx + idx];
  }
  mask &= mask - 1;  // 清除最低位
}
```

## 设计权衡

| 方案 | 优点 | 缺点 |
|------|------|------|
| **flat_hash_map** | Cache 友好<br>SIMD 加速<br>内存占用低 | 删除复杂（需标记）<br>rehash 时内存需求高 |
| **unordered_map** | 删除简单<br>稳定性好（迭代器） | 链表遍历慢<br>内存碎片多 |
| **node_hash_map** | 指针稳定 | 双重间接访问<br>额外内存开销 |

## 适用场景

### ✅ flat_hash_map 适用场景：
- 读多写少的工作负载
- Key/Value 都是 std::string（或小对象）
- 不需要指针稳定性
- 内存敏感的应用

### ❌ 不适用场景：
- 频繁删除的场景（会积累删除标记）
- 需要保留迭代器引用的场景
- Key/Value 是大对象（move 语义无优势）

## Swiss Table vs 标准库

| 特性 | flat_hash_map | unordered_map |
|------|--------------|---------------|
| **冲突处理** | Open addressing | Chaining |
| **探测策略** | Group probing (SIMD) | Linked list |
| **内存布局** | 连续数组 | Bucket 链表 |
| **删除方式** | Tombstone marking | 直接释放 |
| **迭代器稳定性** | 否（rehash 失效） | 是 |

## 为什么不使用 node_hash_map?

`node_hash_map` 虽然提供了指针稳定性，但：

1. **双重间接访问**：bucket → node → data，两次解引用
2. **内存碎片**：每个元素单独分配
3. **我们的场景**：Redis 命令不保留迭代器引用

```cpp
// flat_hash_map - 单次解引用
auto& entry = flat_map.find(key)->second;

// node_hash_map - 双重解引用
auto* node = node_map.find(key);  // 第一次解引用
auto& entry = node->second;       // 第二次解引用
```

## 性能优势来源

### 1. Cache Locality

```
flat_hash_map 内存布局:
[ctrl][ctrl]...[key,val][key,val]...
连续存储，单个 cache line 包含多个元素

unordered_map 内存布局:
bucket[0] → node0 → next node0
bucket[1] → node1 → next node1
链表节点分散在堆上，cache miss 频繁
```

### 2. SIMD 批量探测

```
传统探测（逐个比较）:
for (int i = 0; i < N; ++i) {
  if (control[i] == target) check_entry(i);
}
CPU 指令数: N 次比较 + N 次分支

SIMD 探测（16 个一组）:
mask = _mm_cmpeq_epi8(group, target);
while (mask) {
  int idx = __builtin_ctz(mask);
  check_entry(idx);
  mask &= mask - 1;
}
CPU 指令数: 1 次向量比较 + log(16) 次迭代
```

### 3. 内存占用

```
1000 个 string-string 键值对:

unordered_map:
  - bucket 数组: ~16KB (16 bytes/bucket)
  - node 指针: 8KB (8 bytes/node)
  - next 指针: 8KB (8 bytes/node)
  - 字符串数据: ~60KB
  总计: ~92KB

flat_hash_map:
  - control bytes: 16KB
  - entries 数组: 64KB (64 bytes/entry)
  - 字符串数据: ~60KB
  总计: ~140KB

但考虑到内存碎片和 rehash 效率，实际差距更大
```

## 结论

Swiss Table (flat_hash_map) 的设计完美契合 nano-redis 的需求：

1. **读写性能优异**：SIMD 加速 + Cache 友好
2. **内存效率高**：无链表指针开销
3. **实现简洁**：无需手动管理节点生命周期
4. **适配场景**：Redis 命令短生命周期，无迭代器稳定性需求
