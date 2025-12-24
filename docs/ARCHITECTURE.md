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

