# NanoObj 设计文档

## 概述

`NanoObj` 是 NanoRedis 的核心数据容器，设计为一个**紧凑、高效的通用对象包装器**。它负责存储 Redis 中的各种数据类型，并通过精巧的内存布局实现极致的空间优化，**总大小仅为 16 字节**。

### 核心特性

- **极小内存占用**：固定 16 字节大小，通过精心设计的数据结构实现紧凑存储
- **多类型支持**：字符串、整数、哈希、集合、列表、有序集合等 Redis 数据类型
- **智能编码优化**：根据数据大小自动选择最优存储方式（内联 vs 堆分配）
- **键值优化**：对键进行特殊优化（自动整数转换、前缀缓存）
- **零拷贝语义**：支持移动语义，避免不必要的内存拷贝

---

## 1. 数据结构设计

### 1.1 内存布局

```cpp
class NanoObj {
    uint8_t taglen_;           // 1B: 类型标签或内联字符串长度
    uint8_t flag_;             // 1B: 位图标志
    union U {                  // 14B: 数据存储联合体
        char data[kInlineLen]; // 内联字符串数据（14B）
        SmallString small_str; // 小字符串结构
        RobjWrapper robj;      // Redis 对象包装器
        int64_t ival;         // 整数值（压缩存储）
    } u_;
}; // 总大小: 16 字节

static_assert(sizeof(NanoObj) == 16);
```

### 1.2 Tag 系统

Tag 用于标识 `NanoObj` 存储的数据类型，编码在 `taglen_` 字段中：

```cpp
enum Tag : uint8_t {
    // 0-14: 保留给内联字符串的长度
    // 当 taglen_ ∈ [0, 14] 时，表示内联存储的字符串长度

    INT_TAG = 15,      // 整数类型
    SMALL_STR_TAG = 16, // 小字符串（堆分配）
    ROBJ_TAG = 17,      // Redis 对象（Hash/Set/List/ZSet）
    EXTERNAL_TAG = 18,  // 外部对象（保留）
    JSON_TAG = 19,      // JSON 对象（保留）
    SBF_TAG = 20,       // Bloom Filter（保留）
    NULL_TAG = 31,      // 空对象
};
```

### 1.3 Redis 类型编码

对于复合数据类型，使用 Redis 风格的类型和编码标识：

```cpp
// 类型标识
constexpr uint8_t OBJ_STRING = 0;  // 字符串
constexpr uint8_t OBJ_HASH = 1;    // 哈希
constexpr uint8_t OBJ_SET = 2;     // 集合
constexpr uint8_t OBJ_LIST = 3;    // 列表
constexpr uint8_t OBJ_ZSET = 4;    // 有序集合

// 编码标识
constexpr uint8_t OBJ_ENCODING_RAW = 0;       // 原始字符串
constexpr uint8_t OBJ_ENCODING_INT = 1;      // 整数编码
constexpr uint8_t OBJ_ENCODING_EMBSTR = 8;   // 嵌入式字符串
constexpr uint8_t OBJ_ENCODING_HASHTABLE = 2; // 哈希表
constexpr uint8_t OBJ_ENCODING_SKIPLIST = 7; // 跳表
```

---

## 2. 存储策略

### 2.1 字符串存储策略

根据字符串长度自动选择最优存储方式：

```
字符串长度 L
    │
    ├─ L ≤ 14 字节
    │  └─> 内联存储（taglen_ = L，数据存在 u_.data）
    │       优势：无额外堆分配，访问最快
    │
    └─ L > 14 字节
       └─> SmallString 存储（taglen_ = SMALL_STR_TAG）
            ├─ ptr: 8B - 指向堆分配的字符串
            ├─ length: 2B - 字符串长度
            └─ prefix: 4B - 前 4 字节缓存（用于比较）
```

#### 内联字符串示例

```cpp
NanoObj obj("hello");
// 内存布局:
// taglen_ = 5 (字符串长度)
// u_.data = {'h', 'e', 'l', 'l', 'o', 0, 0, 0, 0, 0, 0, 0, 0, 0}
```

#### SmallString 示例

```cpp
NanoObj obj("This is a very long string that exceeds 14 bytes");
// 内存布局:
// taglen_ = SMALL_STR_TAG (16)
// u_.small_str.ptr = -> "This is a very long string..." (堆分配)
// u_.small_str.length = 47
// u_.small_str.prefix = {'T', 'h', 'i', 's'}
```

### 2.2 整数存储策略

#### 普通整数

```cpp
NanoObj obj(42);
// 内存布局:
// taglen_ = INT_TAG (15)
// u_.ival = 42
```

#### 键的整数优化

对于键值对中的键，尝试自动优化为整数：

```cpp
// fromKey 智能转换
NanoObj key1 = NanoObj::fromKey("123");    // 自动转换为整数
NanoObj key2 = NanoObj::fromKey("abc123"); // 保持字符串

// 转换规则
// 1. 字符串长度 ≤ 20
// 2. 可以解析为 int64_t
// 3. 满足以上条件则存储为整数，否则存储为字符串
```

**实现细节**：

```cpp
void NanoObj::setStringKey(std::string_view str) {
    if (str.size() <= 20) {
        int64_t ival;
        if (string2ll(str.data(), str.size(), &ival)) {
            setInt(ival);  // 优化为整数
            return;
        }
    }
    setString(str);  // 否则存储为字符串
}
```

### 2.3 复合类型存储

复合类型（Hash、Set、List、ZSet）使用 `RobjWrapper` 存储：

```cpp
struct RobjWrapper {
    void* inner_obj_;  // 指向实际数据结构（如 DashTable、std::vector 等）
    uint32_t sz_;      // 数据大小/元素数量
    uint8_t type_;     // Redis 类型（OBJ_HASH、OBJ_SET 等）
    uint8_t encoding_; // 编码方式
} __attribute__((packed));
static_assert(sizeof(RobjWrapper) == 14);
```

#### 示例：创建一个哈希

```cpp
NanoObj hash = NanoObj::fromHash();
// 内部结构：
// taglen_ = ROBJ_TAG (17)
// u_.robj.type_ = OBJ_HASH (1)
// u_.robj.encoding_ = OBJ_ENCODING_HASHTABLE (2)
// u_.robj.inner_obj_ = nullptr (初始化为空)
// u_.robj.sz_ = 0

// 设置实际的哈希表
DashTable<NanoObj, NanoObj>* ht = new DashTable<NanoObj, NanoObj>();
hash.setObj(ht);
// u_.robj.inner_obj_ = ht
// u_.robj.sz_ = ht->size()
```

---

## 3. 内存布局详解

### 3.1 内存结构图

```
┌─────────────────────────────────────────────────────────────────┐
│                    NanoObj (16 Bytes)                          │
├─────────────────────────────────────────────────────────────────┤
│ Offset │ Field    │ Size │ Description                       │
├────────┼──────────┼──────┼───────────────────────────────────┤
│   0    │ taglen_  │  1B  │ 类型标签或内联字符串长度            │
│   1    │ flag_    │  1B  │ 位图标志（保留）                   │
│   2-15 │ union U  │ 14B  │ 数据存储（根据 taglen_ 解释）     │
└────────┴──────────┴──────┴───────────────────────────────────┘

union U (14B) 的三种解释方式：

1. 内联字符串（taglen_ ∈ [0, 14]）
   ┌──────────────────────────────────────────────────────────────┐
   │ u_.data[0..13] - 14 字节字符数组                             │
   └──────────────────────────────────────────────────────────────┘

2. 小字符串（taglen_ = SMALL_STR_TAG）
   ┌──────────────────────────────────────────────────────────────┐
   │ u_.small_str.ptr      (8B) - 指向堆字符串                   │
   │ u_.small_str.length   (2B) - 字符串长度                      │
   │ u_.small_str.prefix   (4B) - 前4字节缓存                    │
   └──────────────────────────────────────────────────────────────┘

3. Redis 对象（taglen_ = ROBJ_TAG）
   ┌──────────────────────────────────────────────────────────────┐
   │ u_.robj.inner_obj_   (8B) - 指向实际对象                     │
   │ u_.robj.sz_          (4B) - 对象大小/元素数                  │
   │ u_.robj.type_        (1B) - Redis 类型                       │
   │ u_.robj.encoding_    (1B) - 编码方式                        │
   └──────────────────────────────────────────────────────────────┘

4. 整数（taglen_ = INT_TAG）
   ┌──────────────────────────────────────────────────────────────┐
   │ u_.ival  (int64_t) - 8字节，剩余6字节填充（浪费但无影响）    │
   └──────────────────────────────────────────────────────────────┘
```

### 3.2 为什么是 14 字节？

```cpp
constexpr size_t kInlineLen = 14;
```

**设计原因**：

1. **联合体对齐要求**：`int64_t` 需要 8 字节对齐，`SmallString` 的 `ptr` 也需要 8 字节对齐
2. **空间计算**：16 字节总大小 - 1 字节 taglen_ - 1 字节 flag_ = 14 字节可用空间
3. **Redis 实践**：短字符串在 Redis 中非常常见（如键名、短消息），内联存储可以大幅减少堆分配

**权衡**：

- 更大的内联空间 → 堆分配减少，但对象变大
- 更小的内联空间 → 对象更小，但堆分配增加
- **14 字节**是经过经验选择的平衡点

---

## 4. 操作接口

### 4.1 构造函数

```cpp
// 默认构造（NULL 对象）
NanoObj();

// 字符串构造（自动选择存储策略）
NanoObj(std::string_view str);

// 整数构造
NanoObj(int64_t val);
```

### 4.2 静态工厂方法

```cpp
// 从字符串创建（普通值）
NanoObj fromString(std::string_view str);

// 从整数创建
NanoObj fromInt(int64_t val);

// 从键创建（智能优化为整数）
NanoObj fromKey(std::string_view key);

// 创建复合类型
NanoObj fromHash();  // 创建哈希对象
NanoObj fromSet();   // 创建集合对象
NanoObj fromList();  // 创建列表对象
NanoObj fromZset();  // 创建有序集合对象
```

### 4.3 类型查询

```cpp
bool isNull() const;   // 是否为空
bool isInt() const;    // 是否为整数
bool isString() const; // 是否为字符串
bool isHash() const;   // 是否为哈希
bool isSet() const;    // 是否为集合
bool isList() const;   // 是否为列表
bool isZset() const;   // 是否为有序集合
```

### 4.4 值转换

```cpp
// 安全转换（返回 optional）
std::optional<std::string_view> tryToString() const;
std::optional<int64_t> tryToInt() const;

// 强制转换（有默认值）
std::string toString() const;
int64_t asInt() const;

// 直接访问
std::string_view getStringView() const;
int64_t getIntValue() const;
```

### 4.5 复合类型访问

```cpp
// 获取底层对象指针
template<typename T>
T* getObj() const;

// 设置底层对象
template<typename T>
void setObj(T* obj);
```

**使用示例**：

```cpp
NanoObj hash = NanoObj::fromHash();

// 获取底层 DashTable
DashTable<NanoObj, NanoObj>* table = hash.getObj<DashTable<NanoObj, NanoObj>>();

// 操作哈希表
table->Insert(NanoObj::fromKey("field1"), NanoObj::fromKey("value1"));

// 更新大小
hash.setObj(table);  // 会自动更新 sz_
```

---

## 5. 核心算法

### 5.1 字符串存储选择算法

```cpp
void NanoObj::setString(std::string_view str) {
    if (str.size() <= kInlineLen) {
        setInlineString(str);  // 内联存储
    } else {
        setSmallString(str);   // 堆分配
    }
}
```

**复杂度分析**：

| 存储方式 | 时间复杂度 | 空间复杂度 | 说明 |
|---------|-----------|-----------|------|
| 内联存储 | O(L) | O(1) | L ≤ 14，无堆分配 |
| SmallString | O(L) | O(L) | 需要 `operator new` |

### 5.2 键的整数优化算法

```cpp
void NanoObj::setStringKey(std::string_view str) {
    // 条件 1: 长度不超过 20
    if (str.size() <= 20) {
        int64_t ival;
        // 条件 2: 可以解析为 int64_t
        if (string2ll(str.data(), str.size(), &ival)) {
            setInt(ival);  // 优化为整数
            return;
        }
    }
    setString(str);  // 否则存储为字符串
}
```

**优化效果**：

| 场景 | 优化前 | 优化后 | 节省 |
|------|-------|-------|------|
| 短整数键（"123"） | 堆分配 | 内联存储 | ~24 字节 |
| 长整数键（"9223372036854775807"） | SmallString | 内联存储 | ~32 字节 |

### 5.3 比较算法

```cpp
bool NanoObj::operator==(const NanoObj& other) const {
    uint8_t this_tag = getTag();
    uint8_t other_tag = other.getTag();

    // 场景 1: 两者都是整数
    if (this_tag == INT_TAG && other_tag == INT_TAG) {
        return u_.ival == other.u_.ival;
    }

    // 场景 2: 两者都是字符串（内联或 SmallString）
    if ((this_tag <= kInlineLen || this_tag == SMALL_STR_TAG) &&
        (other_tag <= kInlineLen || other_tag == SMALL_STR_TAG)) {
        std::string_view this_str = getRawStringView();
        std::string_view other_str = other.getRawStringView();
        return this_str == other_str;
    }

    // 场景 3: 一个是整数，一个是字符串
    // （需要转换为字符串再比较）
    if (this_tag == INT_TAG && (other_tag <= kInlineLen || other_tag == SMALL_STR_TAG)) {
        std::string other_str_val = other.toString();
        std::string this_str_val = toString();
        return this_str_val == other_str_val;
    }

    if (other_tag == INT_TAG && (this_tag <= kInlineLen || this_tag == SMALL_STR_TAG)) {
        std::string this_str_val = toString();
        std::string other_str_val = other.toString();
        return this_str_val == other_str_val;
    }

    // 其他情况：不相等
    return false;
}
```

**优化点**：

1. **快速路径**：同类型比较直接比较值
2. **前缀优化**：SmallString 的 4 字节前缀用于快速比较
3. **避免分配**：尽量使用 `string_view` 而非 `std::string`

---

## 6. 在 DashTable 中的使用

### 6.1 作为键的优化

NanoObj 作为 DashTable 的键时，其紧凑设计带来了巨大优势：

```cpp
// DashTable 中的定义
using Table = DashTable<NanoObj, NanoObj>;

// 查找操作
Table table;
NanoObj key = NanoObj::fromKey("my_key");  // 自动优化为整数（如可能）
auto value = table.Find(key);
```

**内存占用对比**：

| 实现 | 键大小 | 100万键占用 |
|------|-------|-----------|
| std::string | ~32B | ~32MB |
| NanoObj（内联） | 16B | ~16MB |
| **节省** | **50%** | **16MB** |

### 6.2 Hash 特化

NanoObj 实现了 Hash 特化，用于 DashTable：

```cpp
// 在 unordered_dense.h 中
namespace ankerl::unordered_dense::detail {

template <>
struct hash<NanoObj> {
    auto operator()(const NanoObj& obj) const noexcept {
        if (obj.getTag() <= kInlineLen) {
            // 内联字符串：直接 hash 数据
            return ankerl::unordered_dense::hash<std::string_view>{}(
                std::string_view(obj.u_.data, obj.getTag())
            );
        } else if (obj.getTag() == INT_TAG) {
            // 整数：直接 hash int64_t
            return ankerl::unordered_dense::hash<int64_t>{}(obj.u_.ival);
        } else if (obj.getTag() == SMALL_STR_TAG) {
            // SmallString：先比较前缀
            uint32_t prefix_hash = ankerl::unordered_dense::hash<uint32_t>{}(
                *reinterpret_cast<uint32_t*>(obj.u_.small_str.prefix)
            );
            return prefix_hash;
        }
        return uint64_t{0};
    }
};

}
```

---

## 7. 生命周期管理

### 7.1 拷贝语义

```cpp
// 拷贝构造
NanoObj(const NanoObj& other);

// 拷贝赋值
NanoObj& operator=(const NanoObj& other);
```

**行为**：

- **内联字符串**：`memcpy` 复制数据
- **整数**：复制数值
- **SmallString**：深度拷贝（分配新内存）
- **Redis 对象**：复制元数据，**不**拷贝底层对象（由使用者管理）

### 7.2 移动语义

```cpp
// 移动构造
NanoObj(NanoObj&& other) noexcept;

// 移动赋值
NanoObj& operator=(NanoObj&& other) noexcept;
```

**行为**：

- 直接复制 `taglen_`、`flag_` 和 `u_`
- 将 `other` 重置为 NULL_TAG
- **零额外开销**：比拷贝快很多

### 7.3 析构与清理

```cpp
NanoObj::~NanoObj();

void NanoObj::clear();
```

**清理规则**：

```cpp
void NanoObj::clear() {
    if (getTag() == SMALL_STR_TAG) {
        freeSmallString();  // 释放堆分配的字符串
    } else if (getTag() == ROBJ_TAG) {
        if (u_.robj.inner_obj_ != nullptr) {
            ::operator delete(u_.robj.inner_obj_);  // 释放对象
        }
    }
}
```

---

## 8. 使用示例

### 8.1 基本使用

```cpp
// 创建字符串对象
NanoObj str1 = NanoObj::fromString("hello");
NanoObj str2 = NanoObj::fromString("This is a long string...");

// 创建整数对象
NanoObj num = NanoObj::fromInt(42);

// 类型查询
assert(str1.isString());
assert(num.isInt());

// 值访问
std::string_view sv = str1.getStringView();  // "hello"
int64_t val = num.getIntValue();              // 42
```

### 8.2 数据库操作

```cpp
Database db;

// 存储字符串
db.Set(NanoObj::fromKey("name"), "Alice");
db.Set(NanoObj::fromKey("age"), "30");

// 存储整数（自动优化）
db.Set(NanoObj::fromKey("score"), "100");  // 键 "score" 优化为整数

// 获取值
auto name = db.Get(NanoObj::fromKey("name"));  // std::optional<std::string>
if (name) {
    std::cout << "Name: " << *name << std::endl;
}
```

### 8.3 复合类型操作

```cpp
Database db;

// 创建哈希
NanoObj hash = NanoObj::fromHash();
DashTable<NanoObj, NanoObj>* ht = new DashTable<NanoObj, NanoObj>();
ht->Insert(NanoObj::fromKey("field1"), NanoObj::fromKey("value1"));
hash.setObj(ht);

// 存储哈希
db.Set(NanoObj::fromKey("user:1"), hash);

// 访问哈希
auto value = db.Find(NanoObj::fromKey("user:1"));
if (value && value->isHash()) {
    DashTable<NanoObj, NanoObj>* table = value->getObj<DashTable<NanoObj, NanoObj>>();
    auto field = table->Find(NanoObj::fromKey("field1"));
}
```

### 8.4 集合操作

```cpp
// 创建集合
NanoObj set = NanoObj::fromSet();
std::unordered_set<NanoObj>* s = new std::unordered_set<NanoObj>();
s->insert(NanoObj::fromKey("member1"));
s->insert(NanoObj::fromKey("member2"));
set.setObj(s);
```

---

## 9. 性能特性

### 9.1 内存占用

| 类型 | 存储方式 | 大小 | 额外堆分配 |
|------|---------|------|-----------|
| 短字符串（≤14B） | 内联 | 16B | 0B |
| 长字符串（>14B） | SmallString | 16B | L + 堆开销 |
| 整数 | 内联 | 16B | 0B |
| Hash/Set/List/ZSet | RobjWrapper | 16B | 对象大小 |

### 9.2 操作性能

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造（字符串） | O(L) | L ≤ 14: 内联；L > 14: 堆分配 |
| 构造（整数） | O(1) | 常数时间 |
| 拷贝 | O(L) | L ≤ 14: memcpy；L > 14: 深度拷贝 |
| 移动 | O(1) | 直接拷贝数据 |
| 比较 | O(L) | 使用 string_view，尽可能避免分配 |
| 类型查询 | O(1) | 直接读取 tag |

### 9.3 内存对齐

```cpp
struct NanoObj {
    uint8_t taglen_;     // Offset: 0, Size: 1
    uint8_t flag_;       // Offset: 1, Size: 1
    union U {            // Offset: 2, Size: 14
        ...
    };
};
```

- **对齐要求**：8 字节（因为 `int64_t`）
- **填充**：`u_.ival` 后有 6 字节填充（浪费但不可避免）
- **总大小**：16 字节（完美适配缓存行）

---

## 10. 设计权衡与优化

### 10.1 为什么不使用 std::variant？

| 方案 | 优势 | 劣势 |
|------|------|------|
| **手动 Tag + Union** | - 紧凑布局<br>- 零开销抽象<br>- 精确控制内存 | - 需要手动管理类型安全<br>- 代码复杂度高 |
| std::variant | - 类型安全<br>- 标准库支持<br>- 自动析构 | - 额外开销（索引字段）<br>- 不确定性内存布局<br>- 不够紧凑 |

**选择**：手动 Tag + Union，因为需要极致的紧凑性和性能。

### 10.2 为什么限制最大字符串长度为 65535？

```cpp
struct SmallString {
    char* ptr;       // 8B
    uint16_t length; // 2B - 最大 65535
    char prefix[4];  // 4B
} __attribute__((packed));
```

**权衡**：

- **更小的 length**：节省 2 字节（uint32_t → uint16_t）
- **限制**：单个字符串最大 64KB（足够 Redis 使用）
- **Redis 实践**：Redis 的 `sds` 也支持 64KB 以上的字符串，但场景较少

### 10.3 4 字节前缀的作用

```cpp
char prefix[4];  // 4B - 前 4 字节缓存
```

**优化目的**：

1. **快速比较**：在哈希表中比较时，先比较前缀，可能快速失败
2. **局部性**：前缀在栈上，访问更快
3. **缓存友好**：减少堆访问

**使用场景**：

```cpp
// DashTable 的比较
if (a.u_.small_str.prefix[0] != b.u_.small_str.prefix[0]) {
    return false;  // 快速失败
}
// 继续完整比较
```

---

## 11. 未来扩展方向

### 11.1 已预留的 Tag

```cpp
EXTERNAL_TAG = 18,  // 外部对象（如文件描述符）
JSON_TAG = 19,      // JSON 对象
SBF_TAG = 20,       // Scalable Bloom Filter
```

**可能的用途**：

- **EXTERNAL_TAG**：存储文件句柄、数据库连接等
- **JSON_TAG**：支持 Redis JSON 命令（如 `JSON.GET`）
- **SBF_TAG**：布隆过滤器（`BF.ADD`、`BF.EXISTS` 等）

### 11.2 优化方向

1. **更紧凑的整数存储**：对于小整数（-128~127），可以只使用 1 字节
2. **字符串压缩**：对重复字符串使用字符串驻留（String Interning）
3. **零拷贝序列化**：直接序列化到网络缓冲区
4. **SIMD 优化**：使用 SIMD 指令加速字符串比较

### 11.3 内存池

```cpp
class NanoObjPool {
    // 预分配 SmallString 的内存
    // 减少 operator new 的开销
};
```

---

## 12. 与 Redis 的对比

| 特性 | Redis SDS | NanoObj |
|------|-----------|---------|
| 大小 | 变长 | 固定 16B |
| 编码 | 简单字符串 | 多编码（内联/整数/SmallString） |
| 类型 | 仅字符串 | 多类型支持 |
| 内存开销 | ~4-8 字节头部 | 2 字节头部 + 联合体 |
| 优化点 | 避免频繁 realloc | 智能选择存储方式 |
| 适用场景 | 通用键值存储 | 内存受限的高性能存储 |

---

## 13. 参考资料

- [Redis SDS 实现](https://github.com/redis/redis/blob/unstable/src/sds.h)
- [Dragonfly Compact Object](https://github.com/dragonflydb/dragonfly)
- [Redis 对象系统](https://redis.io/docs/reference/protocol-spec/#resp-3-data-types)
- [Type Punning 和 Union](https://en.cppreference.com/w/cpp/language/union)

---

## 附录：完整 API 参考

### A.1 构造函数

```cpp
NanoObj();                           // 默认（NULL 对象）
explicit NanoObj(std::string_view);  // 字符串
explicit NanoObj(int64_t);           // 整数
NanoObj(const NanoObj&);             // 拷贝构造
NanoObj(NanoObj&&) noexcept;         // 移动构造
~NanoObj();                          // 析构
```

### A.2 赋值运算符

```cpp
NanoObj& operator=(const NanoObj&);
NanoObj& operator=(NanoObj&&) noexcept;
```

### A.3 静态工厂

```cpp
static NanoObj fromString(std::string_view);
static NanoObj fromInt(int64_t);
static NanoObj fromKey(std::string_view);  // 智能优化
static NanoObj fromHash();
static NanoObj fromSet();
static NanoObj fromList();
static NanoObj fromZset();
```

### A.4 类型查询

```cpp
bool isNull() const;
bool isInt() const;
bool isString() const;
bool isHash() const;
bool isSet() const;
bool isList() const;
bool isZset() const;
```

### A.5 值访问

```cpp
std::optional<std::string_view> tryToString() const;
std::optional<int64_t> tryToInt() const;
std::string toString() const;
int64_t asInt() const;
std::string_view getStringView() const;
int64_t getIntValue() const;
```

### A.6 属性访问

```cpp
uint8_t getType() const;      // Redis 类型（OBJ_STRING 等）
uint8_t getEncoding() const;   // 编码方式（OBJ_ENCODING_INT 等）
size_t size() const;           // 数据大小
uint8_t getTag() const;        // 内部 Tag
uint8_t getFlag() const;       // 位图标志
```

### A.7 复合类型操作

```cpp
template<typename T>
T* getObj() const;  // 获取底层对象

template<typename T>
void setObj(T* obj);  // 设置底层对象
```

### A.8 比较运算符

```cpp
bool operator==(const NanoObj&) const;
bool operator!=(const NanoObj&) const;
```

---

**文档版本**：1.0
**最后更新**：2026-01-12
**维护者**：NanoRedis 团队
