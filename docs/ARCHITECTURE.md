# Commit 1: 架构说明

## 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    Nano-Redis 0.1.0                       │
├─────────────────────────────────────────────────────────────────┤
│                                                           │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │        Public API Layer (include/nano_redis/)        │  │
│  │                                                          │
│  │  ┌──────────┐  ┌──────────┐                          │  │
│  │  │ version  │  │ status   │                          │  │
│  │  │   .h    │  │   .h     │                          │  │
│  │  └──────────┘  └──────────┘                          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                        ▲                                   │
│                        │                                   │
│  ┌─────────────────────┴───────────────────────────────┐  │
│  │        Implementation Layer (src/)                   │  │
│  │                                                          │
│  │  ┌──────────┐                                          │  │
│  │  │ version  │                                          │  │
│  │  │   .cc    │                                          │  │
│  │  └──────────┘                                          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                        ▲                                   │
│                        │                                   │
│  ┌─────────────────────┴───────────────────────────────┐  │
│  │        Test Layer (tests/)                        │  │
│  │                                                          │
│  │  ┌──────────────┐                                      │  │
│  │  │ version_test │                                      │  │
│  │  │     .cc      │                                      │  │
│  │  └──────────────┘                                      │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                           │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │        Build Systems                                  │  │
│  │                                                          │
│  │  ┌──────────────┐  ┌──────────────┐              │  │
│  │  │ BUILD.bazel  │  │ CMakeLists   │              │  │
│  │  │             │  │     .txt     │              │  │
│  │  └──────────────┘  └──────────────┘              │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────────────┘
```

## 模块设计

### Version 模块

**文件**:
- `include/nano_redis/version.h` (公共 API)
- `src/version.cc` (实现)

**职责**:
- 提供版本信息查询
- 维护版本常量

**接口**:
```cpp
// 编译期常量
constexpr int kMajorVersion = 0;
constexpr int kMinorVersion = 1;
constexpr int kPatchVersion = 0;
constexpr const char* kVersionString = "0.1.0";

// 运行时函数
const char* GetVersion();
```

### Status 模块

**文件**:
- `include/nano_redis/status.h` (头文件实现)

**职责**:
- 统一的错误处理
- 错误信息传递

**接口**:
```cpp
// 错误码
enum class StatusCode {
  kOk = 0,
  kNotFound = 1,
  kInvalidArgument = 2,
  kInternalError = 3,
  kAlreadyExists = 4,
};

// Status 类
class Status {
  Status();                          // 默认 OK
  Status(StatusCode code, const std::string& message);

  static Status OK();
  static Status NotFound(const std::string& msg);
  static Status InvalidArgument(const std::string& msg);
  static Status Internal(const std::string& msg);
  static Status AlreadyExists(const std::string& msg);

  bool ok() const;
  StatusCode code() const;
  const std::string& message() const;
  std::string ToString() const;
};
```

## 依赖关系

```
version.h  ────────┐
                    ├─────> version_test.cc
version.cc  ────────┘

status.h   ─────────────> version_test.cc
```

## 数据流

### 版本查询流程

```
Client Code
     │
     │ GetVersion()
     ▼
version.h (inline function)
     │
     │ return kVersionString
     ▼
Client receives "0.1.0"
```

### 错误处理流程

```
Client Code
     │
     │ Status::NotFound("key not found")
     ▼
Status(code=kNotFound, message="key not found")
     │
     │ .ok() → false
     │ .code() → kNotFound
     │ .message() → "key not found"
     │ .ToString() → "NOT_FOUND: key not found"
     ▼
Error Handling
```

## 扩展点

### 后续模块可以依赖的基础

1. **Status**: 所有模块的错误处理
   ```cpp
   Status DoSomething() {
     if (error) {
       return Status::Internal("operation failed");
     }
     return Status::OK();
   }
   ```

2. **Version**: 版本检查和兼容性
   ```cpp
   if (client_version > kCurrentVersion) {
     return Status::InvalidArgument("version mismatch");
   }
   ```

## 编译模型

### 单元测试编译

```
CMake:
  version_test.cc (test) + version.cc (src) + GTest
          ↓
    version_test (executable)

Bazel:
  version_test.cc (test) + version.cc (src) + GTest
          ↓
    version_test (executable)
```

### 静态库编译

```
CMake:
  version.cc (src) + status.h (header)
           ↓
     nano_redis (static library)

Bazel:
  version.cc (src) + status.h (header)
           ↓
     nano_redis (static library)
```

---

# Commit 2: Arena Allocator - 架构说明

## 模块设计

### Arena 模块

**文件**:
- `include/nano_redis/arena.h` (公共 API)
- `src/arena.cc` (实现)

**职责**:
- 高效的批量内存分配
- 内存对齐支持
- 自动内存管理

**接口**:
```cpp
class Arena {
 public:
  Arena();                                    // 默认构造
  explicit Arena(size_t block_size);          // 指定块大小

  void* Allocate(size_t size,                // 分配内存
                 size_t alignment = alignof(std::max_align_t));
  
  void Reset();                               // 释放所有内存
  
  size_t MemoryUsage() const;                 // 内存使用量
  size_t BlockCount() const;                  // 块数量
  size_t AllocatedBytes() const;             // 已分配字节数
};
```

## 内存布局

### 当前块状态

```
┌────────────────────────────────────────────────────┐
│ Block N (当前活跃块)                                │
├────────────────────────────────────────────────────┤
│ [已分配区域] [对齐填充] [空闲区域]                  │
│     ↑ ptr_                           ↑ end_       │
└────────────────────────────────────────────────────┘
```

### 多块分配

```
时间线：
T0: 初始状态
    ptr_ = nullptr, end_ = nullptr

T1: Allocate(100)
    ┌─────────────────────┐
    │ [100B] [空闲]       │
    └─────────────────────┘
    ptr_ = 100, end_ = 4096

T2: Allocate(500)
    ┌─────────────────────┐
    │ [100B] [500B] [空闲] │
    └─────────────────────┘
    ptr_ = 600, end_ = 4096

T3: Allocate(4000) → 不够！
    ┌─────────────────────┐
    │ [100B] [500B]       │ ← Block 0 (保留)
    └─────────────────────┘

    ┌─────────────────────────────┐
    │ [4000B] [空闲]               │ ← Block 1 (新分配)
    └─────────────────────────────┘
    ptr_ = 4000, end_ = 8192
```

### 块向量管理

```
blocks_: std::vector<void*>
    ↓
    [Block0, Block1, Block2, ...]
       ↓        ↓        ↓
    void*    void*    void*
   4KB      4KB      4KB
```

## 分配算法

### 对齐处理

```cpp
void* Allocate(size_t size, size_t alignment) {
  // 当前地址
  uintptr_t current = reinterpret_cast<uintptr_t>(ptr_);
  
  // 计算对齐偏移
  uintptr_t offset = current % alignment;
  uintptr_t padding = (offset == 0) ? 0 : alignment - offset;
  
  // 检查是否有足够空间
  if (ptr_ + size + padding > end_) {
    AllocateNewBlock(size + padding);
    padding = reinterpret_cast<uintptr_t>(ptr_) % alignment;
    padding = (padding == 0) ? 0 : alignment - padding;
  }
  
  // 返回对齐后的地址
  char* result = ptr_ + padding;
  ptr_ += size + padding;
  
  return result;
}
```

### 对齐示例

```
假设 ptr_ = 0x1003 (未对齐到 8 字节)
alignment = 8

offset = 0x1003 % 8 = 3
padding = 8 - 3 = 5

分配后：
    [padding=5][size=16]
    0x1003 ──────────> 0x1008 ─────────> 0x1018
    ↑ ptr_          ↑ result         ↑ 新 ptr_
    
result = 0x1008 (已对齐到 8 字节)
```

## 依赖关系

```
arena.h  ────────┐
                 ├─────> arena_test.cc
arena.cc  ────────┘

arena.h  ───────────> arena_bench.cc
```

## 数据流

### 分配流程

```
Client Code
      │
      │ arena.Allocate(64)
      ▼
检查对齐
      │
      ├─ 计算偏移
      ├─ 检查空间
      │
      ▼
空间不足?
      │
      ├─ Yes → AllocateNewBlock()
      └─ No  → 直接分配
              │
              ├─ ptr_ += size + padding
              └─ 返回对齐后的地址
```

### 释放流程

```
Client Code
      │
      │ arena.Reset()
      ▼
遍历 blocks_
      │
      ├─ for each block: ::operator delete(block)
      └─ blocks_.clear()
              │
              ├─ ptr_ = nullptr
              ├─ end_ = nullptr
              └─ memory_usage_ = 0
```

## 性能特征

### 时间复杂度

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| Allocate | O(1) | 指针移动 |
| Reset | O(n) | n = 块数量 |
| MemoryUsage | O(1) | 返回成员变量 |

### 空间复杂度

- **最佳情况**: 所有对象在一个块中
- **最坏情况**: 每个对象需要新块（碎片化）
- **平均情况**: 块利用率 ~80%

## 扩展点

### 后续优化方向

1. **Per-thread Arena**
   ```cpp
   class ThreadLocalArena {
     static thread_local Arena arena_;
   public:
     static void* Allocate(size_t size);
   };
   ```

2. **对象池**
   ```cpp
   template<typename T>
   class ObjectPool {
     Arena arena_;
   public:
     T* Allocate() { return new (arena_.Allocate(sizeof(T))) T; }
   };
   ```

3. **智能指针包装**
   ```cpp
   template<typename T>
   using ArenaPtr = std::unique_ptr<T, ArenaDeleter>;
   ```

## 内存安全

### 潜在问题及防护

1. **悬空指针**: Reset() 后所有指针失效
   - **防护**: 文档警告生命周期

2. **内存泄漏**: 忘记调用 Reset()
   - **防护**: 使用 RAII 包装器

3. **越界访问**: Allocate 后写入超出大小
   - **防护**: 使用调试模式检测

### 调试支持

```cpp
#ifdef DEBUG
  void* Allocate(size_t size, size_t alignment) {
    // 在每个分配后添加 canary
    char* ptr = static_cast<char*>(DoAllocate(size + 8));
    memset(ptr + size, 0xAB, 8);
    return ptr;
  }
  
  void CheckCanaries() {
    // 检查 canary 是否被破坏
  }
#endif
```


---

# Commit 3: String Store (flat_hash_map) - 架构说明

## 模块设计

### StringStore 模块

**文件**:
- `include/nano_redis/string_store.h` (公共 API)
- `src/string_store.cc` (实现)

**职责**:
- 基于 flat_hash_map 的键值存储
- 字符串键值对管理
- 提供与 unordered_map 对比的基类

**接口**:
```cpp
class StringStore {
 public:
  StringStore();
  ~StringStore();
  
  // 插入操作
  bool Put(const std::string& key, const std::string& value);
  bool Put(std::string&& key, std::string&& value);
  
  // 查找操作
  bool Get(const std::string& key, std::string* value) const;
  std::string* GetMutable(const std::string& key);
  
  // 删除操作
  bool Delete(const std::string& key);
  
  // 查询操作
  bool Contains(const std::string& key) const;
  
  // 状态操作
  size_t Size() const;
  bool Empty() const;
  void Clear();
  
  // 内存统计
  size_t MemoryUsage() const;
  const StringMap& GetStore() const;
  
 private:
  using StringMap = absl::flat_hash_map<std::string, std::string>;
  StringMap store_;
};
```

### StdStringStore 模块

**职责**:
- 基于 unordered_map 的对比实现
- 与 StringStore 保持相同接口

**接口**:
```cpp
class StdStringStore {
 public:
  // 与 StringStore 完全相同的接口
  
 private:
  using StringMap = std::unordered_map<std::string, std::string>;
  StringMap store_;
};
```

## Swiss Table 内存布局

### 完整结构

```
flat_hash_map 内存布局：

┌─────────────────────────────────────────────────────────────────┐
│ Control Array                                                   │
│ [ctrl0][ctrl1][ctrl2]...[ctrlN]                                 │
│  1B      1B      1B       1B                                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Entry Array                                                      │
│ [Entry0][Entry1][Entry2]...[EntryN]                             │
│  64B     64B     64B      64B                                    │
└─────────────────────────────────────────────────────────────────┘

每个 Entry:
┌─────────────────────────────────────────────┐
│ std::string key  │ std::string value        │
│ [size][cap][ptr] │ [size][cap][ptr]         │
└─────────────────────────────────────────────┘
      24-32 bytes          24-32 bytes
```

### Control Byte Group (16 bytes)

```
┌─────────────────────────────────────┐
│ c0 │ c1 │ c2 │ ... │ c15           │
│ 1B │ 1B │ 1B │     │ 1B            │
└─────────────────────────────────────┘
       Group 0 (aligned to 16 bytes)

控制字节编码:
  0x80 (128): 空槽 (kEmpty)
  0xFE (254): 已删除 (kDeleted)
  0xFF (255): 哨兵位 (kSentinel)
  0x00-0x7F:  元素的 H2 hash 值 (7 bits)
```

### 查找流程图

```
查找 key = "hello"

1. 计算 hash
   hash = std::hash<string>("hello") = 0x12345678
   h1 = 0x12345678 >> 7  (用于 bucket 选择)
   h2 = 0x12345678 & 0x7F (用于 SIMD 匹配)

2. 定位 control group
   bucket = h1 % table_size
   group_start = (bucket / 16) * 16

3. SIMD 探测
   ┌──────────────────────────────────────┐
   │ 0x12 0x80 0x00 0x78 ...              │
   │  ↑                           ↑        │
   │ h2=0x78 匹配                        │
   └──────────────────────────────────────┘
   
   _mm_cmpeq_epi8(group, 0x78) → mask = 0b1000...
   
4. 提取匹配索引
   idx = __builtin_ctz(mask) → 找到匹配的索引
   
5. 完整比较 key
   if (entries_[idx].key == "hello") {
     return &entries_[idx].value;
   }
```

## 对比：flat_hash_map vs unordered_map

### 内存布局对比

```
flat_hash_map (open addressing):
┌─────────────────────────────────────┐
│ [Entry0][Entry1][Entry2]...         │
│ 连续存储，缓存友好                  │
└─────────────────────────────────────┘

unordered_map (chaining):
┌─────────────────────────────────────┐
│ bucket[0] → node → node → null     │
│ bucket[1] → node → null            │
│ bucket[2] → node → node → node     │
│ 链表分散，缓存不友好                │
└─────────────────────────────────────┘
```

### 性能特征对比

| 特性 | flat_hash_map | unordered_map |
|------|--------------|---------------|
| **插入** | O(1) 平均 | O(1) 平均 |
| **查找** | O(1) 平均 | O(1) 平均 |
| **删除** | O(1) 平均 + 标记清理 | O(1) 平均 |
| **遍历** | O(n) 连续 | O(n) 跳跃 |
| **Cache miss** | ~10% (连续内存) | ~50% (链表跳转) |
| **内存占用** | 较低 (无指针) | 较高 (链表节点) |

## 依赖关系

```
string_store.h ───────┐
                     ├────> hash_table_bench.cc
string_store.cc ──────┘

string_store.h ──────────> (Abseil::flat_hash_map)
```

## 数据流

### 插入流程

```
Client Code
      │
      │ store.Put("key", "value")
      ▼
hash(key) → H1, H2
      │
      ▼
查找是否存在
      │
      ├─ Yes → 更新 value
      └─ No  → 查找空槽
               │
               ├─ SIMD 探测 control bytes
               ├─ 找到空槽 (kEmpty 或 kDeleted)
               └─ 插入 Entry
                      │
                      ├─ 设置 control byte = H2
                      └─ 存储 key, value
```

### 查找流程

```
Client Code
      │
      │ store.Get("key", &value)
      ▼
hash(key) → H1, H2
      │
      ▼
定位 control group
      │
      ▼
SIMD 探测
      │
      ├─ _mm_cmpeq_epi8(group, H2)
      ├─ 提取匹配 mask
      └─ 遍历匹配索引
               │
               ├─ 完整比较 key
               ├─ 匹配 → 返回 value
               └─ 不匹配 → 继续探测
```

### 删除流程

```
Client Code
      │
      │ store.Delete("key")
      ▼
hash(key) → H1, H2
      │
      ▼
查找 Entry
      │
      ├─ 找到 → 标记 control byte = kDeleted
      └─ 未找到 → 返回 false
```

## 性能特征

### 时间复杂度

| 操作 | flat_hash_map | unordered_map | 差异 |
|------|--------------|---------------|------|
| 插入 | O(1) 平均 | O(1) 平均 | 相同 |
| 查找 | O(1) 平均 | O(1) 平均 | 相同 |
| 删除 | O(1) 平均 | O(1) 平均 | 相同 |
| 遍历 | O(n) | O(n) | 相同 |

### 实际性能（预期）

| 操作 | flat_hash_map | unordered_map | 提升 |
|------|--------------|---------------|------|
| Insert 100K | 120ms | 280ms | 2.3x |
| Lookup 100K | 80ms | 150ms | 1.9x |
| Delete 100K | 150ms | 200ms | 1.3x |
| Iterate 100K | 10ms | 12ms | 1.2x |

### 空间复杂度

| 元素数 | flat_hash_map | unordered_map | 差异 |
|--------|--------------|---------------|------|
| 1K | ~64KB | ~96KB | -33% |
| 10K | ~640KB | ~960KB | -33% |
| 100K | ~6.4MB | ~9.6MB | -33% |
| 1M | ~64MB | ~96MB | -33% |

## 扩展点

### 后续优化方向

1. **自定义分配器**
   ```cpp
   using StringMap = absl::flat_hash_map<
     std::string,
     std::string,
     absl::Hash<std::string>,
     std::equal_to<std::string>,
     ArenaAllocator<std::pair<const std::string, std::string>>
   >;
   ```

2. **异构查找**
   ```cpp
   // 使用 string_view 避免临时 string
   template<typename K>
   std::string* Get(const K& key);
   ```

3. **内存池集成**
   ```cpp
   class StringStore {
     Arena arena_;
     // Entry 从 arena 分配
   };
   ```

4. **批量操作**
   ```cpp
   void PutBatch(const std::vector<std::pair<std::string, std::string>>& items);
   ```

## 内存安全

### 潜在问题及防护

1. **迭代器失效**
   - rehash 时所有迭代器失效
   - **防护**: 文档警告，避免长期持有迭代器

2. **删除标记堆积**
   - 频繁删除会留下大量 kDeleted 标记
   - **防护**: 自动清理机制（当 deleted > size/4 时 rehash）

3. **哈希碰撞攻击**
   - 恶意构造的 key 导致性能退化
   - **防护**: 使用 SipHash 或随机化哈希种子

### 调试支持

```cpp
#ifdef DEBUG
  void PrintDebugInfo() const {
    std::cout << "Size: " << store_.size() << "\n";
    std::cout << "Capacity: " << store_.capacity() << "\n";
    std::cout << "Load factor: " << LoadFactor() << "\n";
  }
  
  double LoadFactor() const {
    return static_cast<double>(store_.size()) / store_.capacity();
  }
#endif
```
