# Commit 1: Hello World - 架构说明

## 项目结构

```
nano_redis/
├── include/nano_redis/    # 公共头文件（稳定接口）
│   ├── version.h          # 版本号
│   └── status.h           # 错误处理
├── src/                   # 实现文件
│   └── version.cc         # 版本实现
├── tests/                 # 单元测试
│   └── version_test.cc    # 版本测试
├── docs/                  # 文档
│   ├── DESIGN.md          # 设计决策
│   ├── ARCHITECTURE.md    # 架构说明
│   ├── PERFORMANCE.md      # 性能分析
│   └── LESSONS_LEARNED.md # 学习要点
├── CMakeLists.txt         # CMake 构建
└── README.md              # 项目说明
```

## 模块依赖关系

```
version.h
  ├── status.h (错误处理)
  └── version.cc (实现)
      └── version_test.cc (测试)
```

## 数据流图

```
用户调用 GetVersion()
  ↓
返回 kVersionString (编译期常量)
  ↓
用户获取版本信息
```

## 编译流程

```bash
CMakeLists.txt
  ↓
配置编译选项 (C++17, -Wall -Wextra -Werror)
  ↓
添加源文件 (version.cc)
  ↓
链接 Abseil (flat_hash_map)
  ↓
生成可执行文件 (version_test)
  ↓
运行测试
```

---

# Commit 2: Arena Allocator - 架构说明

## Arena 内存布局

```
┌─────────────────────────────────────────────────────┐
│ Arena 类                                          │
├─────────────────────────────────────────────────────┤
│ char* ptr_         // 当前分配位置                  │
│ char* end_         // 当前块结束位置                │
│ size_t block_size_ // 每块大小 (默认 4KB)           │
│ vector<void*> blocks_  // 已分配的块               │
└─────────────────────────────────────────────────────┘

内存布局：

Block 0 (4KB):
┌─────────────────────────────────────────┐
│ [已分配区域] [空闲区域]                 │
│    ↑ptr_              ↑end_              │
└─────────────────────────────────────────┘

Block 1 (4KB):  ┐
                │ blocks_ 向量
Block 2 (4KB):  │
                │
```

## 分配流程

```
Allocate(size, alignment)
  ↓
检查当前块是否有足够空间
  ↓ 否
分配新的 Block (block_size_)
  ↓
计算对齐所需的填充 (padding)
  ↓
ptr_ += padding + size
  ↓
返回分配的地址
```

## 释放流程

```
Reset()
  ↓
遍历 blocks_ 向量
  ↓
free(block)  // 释放每个块
  ↓
blocks_.clear()
  ↓
ptr_ = end_ = nullptr
```

## 内存对齐处理

```
┌─────────────────────────────────────────┐
│ 未对齐的指针：0x1003                    │
│ 要求对齐：8 字节                        │
│                                          │
│ offset = 0x1003 % 8 = 3                │
│ padding = 8 - 3 = 5                     │
│                                          │
│ 对齐后的指针：0x1008                    │
│                                          │
│ 填充：[XXX......]                        │
│      ↑  ↑                              │
│   padding 原地址                        │
└─────────────────────────────────────────┘
```

---

# Commit 3: flat_hash_map vs unordered_map - 架构说明

## flat_hash_map (Swiss Table) 内存布局

```
┌───────────────────────────────────────────────────────────────────┐
│ flat_hash_map 内部结构                                             │
├───────────────────────────────────────────────────────────────────┤
│ Control Array │ Metadata  │   Entry 0    │   Entry 1   │ ...   │
│ [Ctrl0...Ctrl15] [hashes] [key0,val0] [key1,val1] ...          │
└───────────────────────────────────────────────────────────────────┘
  Group 0 (16 bytes)

Control Byte 结构：
┌─────────────────────────────────────────┐
│ 7 6 5 4 3 2 1 0  (bit)               │
│ └─────┬──────┘  (H2 hash, 7 bits)    │
│       │                               │
│     MSB (特殊标志位)                   │
│     0: Full slot                       │
│     1: Empty/Deleted/Sentinel          │
└─────────────────────────────────────────┘

特殊值：
- kEmpty = 0x80 (10000000)  - 空槽
- kDeleted = 0xFE (11111110) - 已删除
- kSentinel = 0xFF (11111111) - 哨兵位
```

## 查找流程

```
查找 key:
  1. hash = std::hash(key)
  2. H1 = hash >> 7  (用于 bucket 选择)
  3. H2 = hash & 0x7F  (用于 SIMD 匹配)
  4. bucket = H1 % table_size
  5. group_start = (bucket / 16) * 16
  6. 使用 SIMD 比较 group_start 附近的 16 个 control bytes
  7. 找到匹配的 H2 值
  8. 完整比较 key
  9. 返回 value 或继续探测
```

## SIMD 探测流程

```
┌─────────────────────────────────────────┐
│ SIMD 探测（16 个槽同时比较）            │
├─────────────────────────────────────────┤
│ 加载 16 个 control bytes:               │
│   [0x80, 0x3A, 0x80, 0x5B, ...]        │
│                                          │
│ 设置目标值（H2）:                       │
│   [0x3A, 0x3A, 0x3A, 0x3A, ...]        │
│                                          │
│ 比较向量:                                │
│   [0, 1, 0, 0, ...]                    │
│     ↑                                   │
│   找到匹配                              │
└─────────────────────────────────────────┘

提取匹配掩码:
  mask = _mm_movemask_epi8(cmp)
  // mask 的每个 bit 对应一个槽

遍历匹配的槽:
  while (mask) {
    idx = __builtin_ctz(mask)  // 最低位索引
    if (CheckFullMatch(idx, key)) {
      return &entries_[idx];
    }
    mask &= mask - 1  // 清除最低位
  }
```

## StringStore 架构

```
┌─────────────────────────────────────────┐
│ StringStore                             │
├─────────────────────────────────────────┤
│ flat_hash_map<                      │
│   std::string,  // key                │
│   std::string   // value              │
│ > store_;                              │
└─────────────────────────────────────────┘

操作流程：

Put(key, value):
  1. 计算哈希
  2. 查找 bucket
  3. 如果存在，更新 value
  4. 如果不存在，插入新 entry
  5. 可能触发 rehash

Get(key, value):
  1. 计算哈希
  2. 查找 bucket
  3. SIMD 探测
  4. 完整比较 key
  5. 返回 value
```

---

# Commit 4: 字符串处理 - std::string 高效使用

## std::string 内部结构

### GCC/Clang 实现

```
┌─────────────────────────────────────────────────────────────┐
│ std::string 内部结构（简化）                                  │
├─────────────────────────────────────────────────────────────┤
│ union {                                                     │
│   // 长字符串模式（堆分配）                                  │
│   struct LongData {                                         │
│     char* ptr;        // 指向堆内存 (8 bytes)               │
│     size_t size;      // 字符串长度 (8 bytes)                │
│     size_t capacity;  // 容量 (8 bytes)                      │
│   } long_;                                                  │
│                                                             │
│   // 短字符串模式（SSO）                                    │
│   struct ShortData {                                        │
│     char data[16];    // 内联缓冲区 (16 bytes)              │
│     unsigned char size; // 字符串长度 (1 byte, MSB=0 表示SSO) │
│   } short_;                                                 │
│ };                                                          │
└─────────────────────────────────────────────────────────────┘

判断模式：
  is_short() = (short_.size & 0x80) == 0
```

### SSO 内存布局

```
短字符串（SSO, ≤15 字节）：
┌────────────────────────────────────────────────────┐
│ [15 字节字符数据] [1 字节长度 MSB=0]              │
│  data[0..14]      size (bit 0-6)                │
└────────────────────────────────────────────────────┘
  总大小：16 字节（栈上分配）

长字符串（>15 字节）：
┌────────────────────────────────────────────────────┐
│ [堆指针]     [8 字节长度]   [8 字节容量]          │
│  long_.ptr    long_.size    long_.capacity       │
└────────────────────────────────────────────────────┘
  总大小：24 字节（栈上）+ 堆内存

堆内存：
┌────────────────────────────────────────────────────┐
│ [实际字符数据...]                                 │
└────────────────────────────────────────────────────┘
```

## StringUtils 架构

```
┌─────────────────────────────────────────────────────────┐
│ StringUtils 类（静态方法）                              │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ 查询操作（零拷贝）                                 │ │
│ │ - StartsWithIgnoreCase(s, prefix)                  │ │
│ │ - EndsWithIgnoreCase(s, suffix)                    │ │
│ │ - CompareIgnoreCase(a, b)                          │ │
│ │ - FindIgnoreCase(haystack, needle)                │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ 转换操作                                             │ │
│ │ - ToLower(s)           // 原地修改                │ │
│ │ - ToLowerCopy(s)       // 返回新字符串（move）     │ │
│ │ - ToUpper(s)           // 原地修改                │ │
│ │ - ToUpperCopy(s)       // 返回新字符串（move）     │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ 修剪操作（原地修改）                                 │ │
│ │ - TrimLeft(s)                                     │ │
│ │ - TrimRight(s)                                    │ │
│ │ - Trim(s)                                         │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ 分割和连接（预分配优化）                             │ │
│ │ - Split(s, delim)      // 预估大小，避免重新分配    │ │
│ │ - Join(parts, delim)   // 预计算总大小              │ │
│ │ - Concat(parts)        // 使用 append              │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ 转义操作                                             │ │
│ │ - Escape(s)            // 转义特殊字符              │ │
│ │ - Unescape(s)          // 反转义                    │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ 类型转换                                             │ │
│ │ - IntToString(value, base)  // 支持不同进制        │ │
│ │ - StringToInt(s, *value)   // 安全解析            │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
│ ┌───────────────────────────────────────────────────┐ │
│ │ SSO 工具                                             │ │
│ │ - SSOThreshold()          // 获取 SSO 阈值          │ │
│ │ - IsSSO(s)                // 检查是否使用 SSO      │ │
│ └───────────────────────────────────────────────────┘ │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## 关键操作流程

### ToLower 原地修改

```
ToLower(s):
  for each char in s:
    if char is uppercase (A-Z):
      char = char + 32  // 转换为小写
    else:
      unchanged

优势：
  - 无额外内存分配
  - 原地修改，速度快
```

### ToLowerCopy 返回新字符串

```
ToLowerCopy(s):
  // 参数按值传递（可能的拷贝）
  result = s
  for each char in result:
    if char is uppercase (A-Z):
      char = char + 32
  return result  // RVO 优化

优势：
  - 调用者可以保留原始字符串
  - 使用 RVO 避免拷贝
```

### Split 分割

```
Split(s, delim):
  1. 预估结果大小：count = count(delim) + 1
  2. result.reserve(count)  // 预分配
  3. 遍历字符串：
       - 找到 delim 位置
       - substr(start, pos - start)
       - push_back(result)  // RVO
  4. 添加最后一个部分
  5. return result  // RVO

优化：
  - 预分配避免多次重新分配
  - substr 对于小字符串使用 SSO
  - RVO 避免返回值拷贝
```

### Join 连接

```
Join(parts, delim):
  1. 预计算总大小：
       total = sum(part.size()) + delim.size() * (parts.size() - 1)
  2. result.reserve(total)  // 一次性分配
  3. 遍历 parts：
       - if i > 0: result += delim
       - result += parts[i]  // 直接追加
  4. return result  // RVO

优化：
  - 预计算总大小，一次性分配
  - 避免多次重新分配
  - 使用 += 比 append 快
```

### Concat 拼接

```
Concat(parts):
  1. 预计算总大小：total = sum(part.size())
  2. result.reserve(total)  // 一次性分配
  3. for part in parts:
       result.append(part)  // 使用 append 比 += 稍快
  4. return result  // RVO

优化：
  - 预分配内存
  - 使用 append（避免额外检查）
  - RVO 避免拷贝
```

## 字符串拷贝优化示例

### 场景 1：值传递 vs 引用传递

```
❌ 低效：值传递（拷贝）
void Process(std::string s) {
  // s 是拷贝，触发堆分配（如果长字符串）
}

✅ 高效：引用传递（无拷贝）
void Process(const std::string& s) {
  // s 是引用，无拷贝
}

性能差异：
  - 短字符串（SSO）：~5ns vs ~0ns
  - 长字符串（堆）：~100ns vs ~0ns
```

### 场景 2：使用 std::move

```
❌ 低效：拷贝赋值
std::string src = "Hello, World!";
std::string dest;
dest = src;  // 深拷贝，~100ns

✅ 高效：移动赋值
std::string src = "Hello, World!";
std::string dest;
dest = std::move(src);  // 转移所有权，~5ns

性能差异：
  - 短字符串（SSO）：~5ns vs ~5ns（相同）
  - 长字符串（堆）：~5ns vs ~100ns（20x）
```

### 场景 3：避免临时字符串

```
❌ 低效：创建临时字符串
std::string result;
for (const auto& item : items) {
  result += "[" + item + "]";  // 创建临时字符串
}

✅ 高效：直接追加
std::string result;
result.reserve(items.size() * 20);  // 预分配
for (const auto& item : items) {
  result += "[";
  result += item;
  result += "]";  // 无临时字符串
}

性能差异：
  - 100 次循环：~250ms vs ~50ms（5x）
```

## 内存使用对比

### 场景：存储 100K 字符串

```
小字符串（平均 8 字符）：
  ┌────────────────────────────────────────┐
  │ SSO 模式（栈上）                       │
  │ - 每个 string: 16 bytes               │
  │ - 总内存: 1.6 MB                      │
  │ - 缓存友好: L1 cache                  │
  └────────────────────────────────────────┘

  ┌────────────────────────────────────────┐
  │ 堆分配模式                            │
  │ - 每个 string: 24 bytes (栈) + 8 bytes (堆) │
  │ - 总内存: 3.2 MB (栈) + 0.8 MB (堆)   │
  │ - 缓存不友好: L2/L3 cache            │
  └────────────────────────────────────────┘

  节省：50% 内存

大字符串（平均 100 字符）：
  ┌────────────────────────────────────────┐
  │ 堆分配模式                            │
  │ - 每个 string: 24 bytes (栈) + 100 bytes (堆) │
  │ - 总内存: 2.4 MB (栈) + 10 MB (堆)    │
  │ - SSO 不适用                          │
  └────────────────────────────────────────┘

  节省：0%（SSO 不适用）
```

## 性能优化总结

```
优化技术                    | 提升倍数 | 适用场景
----------------------------|---------|----------
SSO（小字符串）             | 20x     | ≤15 字符
std::move（长字符串）       | 20x     | >15 字符
const& 引用传递             | ∞       | 只读参数
reserve 预分配             | 2-5x    | 已知大小
Split/Join 预分配           | 2.5x    | 分割/连接操作
避免临时字符串              | 5x      | 循环拼接
RVO/NRVO 返回值优化        | ∞       | 函数返回
