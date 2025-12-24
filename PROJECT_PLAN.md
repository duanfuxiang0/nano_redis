# Nano-Redis 2025 - 完整教程方案

## 🔧 技术栈调整说明（2025-01）

**主要调整**：
- ✅ C++20 → C++17（提高兼容性）
- ✅ Bazel + CMake → 仅 CMake（简化构建）
- ✅ C++20 协程 → 自实现 Task<T>（可控执行模型）
- ✅ Abseil string_view/Cord → std::string（简化依赖）
- ✅ Abseil Time → std::chrono（标准库）
- ✅ Abseil 通过 add_subdirectory 引入（本地目录）

**保留技术**：
- io_uring（高性能 I/O）
- Abseil flat_hash_map/flat_hash_set/InlinedVector（高效容器）
- Arena Allocator（内存优化）
- Time Wheel（O(1) 定时器）

### Abseil 集成方式

```cmake
# CMakeLists.txt
set(ABSL_PROPAGATE_CXX_STD ON)

# 设置 Abseil 路径（可通过环境变量覆盖）
if(DEFINED ENV{ABSEIL_PATH})
  set(ABSEIL_ROOT $ENV{ABSEIL_PATH})
else()
  set(ABSEIL_ROOT ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp)
endif()

add_subdirectory(${ABSEIL_ROOT} ${CMAKE_BINARY_DIR}/abseil-cpp EXCLUDE_FROM_ALL)

# 只链接需要的组件
target_link_libraries(nano_redis
  PUBLIC
    absl::flat_hash_map
    absl::flat_hash_set
    absl::inlined_vector
)
```

---

## 📊 项目概览

| 指标 | 目标 |
|------|------|
| **总提交数** | ~18 个 |
| **有效代码行** | ≤ 5000 行 |
| **每个提交** | 200-350 行新增代码 |
| **学习曲线** | 渐进式，从简单到复杂 |
| **测试覆盖** | 每个提交都有对应的测试 |
| **C++标准** | C++17 |
| **构建系统** | CMake |
| **异步模型** | 自实现 task<T> |

---

## 📝 18 个 Git 提交课程大纲

### 🟢 第一阶段：基础设施（提交 1-5）

---

#### **Commit 1: Hello World - 项目脚手架** ✅ 已完成

**学习目标**:
- 理解现代 C++ 项目结构
- 学习如何选择性使用 Abseil
- 建立测试框架

**设计决策**:
1. 为什么只选 CMake?
    - 简化项目结构
    - 广泛支持，易于部署
    - 学习成本低

2. 目录结构为什么这样设计?
    - 分离头文件和实现（清晰的接口）
    - 按功能模块化（易于学习）

**文件清单**:
```
include/nano_redis/version.h    # 版本号
include/nano_redis/status.h     # 错误处理
src/version.cc                 # 版本实现
tests/version_test.cc          # 版本测试
CMakeLists.txt                # CMake 构建
README.md                     # 项目说明
docs/DESIGN.md               # 设计决策
docs/ARCHITECTURE.md          # 架构说明
docs/PERFORMANCE.md            # 性能分析
docs/LESSONS_LEARNED.md        # 学习要点
```

**代码量**: ~180 行

**关键技术点**:
- `constexpr` 编译期常量（C++11+）
- `inline` 函数优化
- `enum class` 强类型枚举
- GoogleTest 断言
- Status 错误处理模式
- CMake 构建系统

**测试**: GET 6/6 PASSED

---

#### **Commit 2: 内存分配器 - Arena 设计**

**学习目标**:
- 理解 Arena Allocator 的原理
- 学习如何避免频繁的 malloc/free
- 对比 Arena vs 直接 malloc 的性能

**设计决策**:
1. 为什么用 Arena 而不是直接 malloc?
    - 减少系统调用次数（批量分配）
    - 缓存局部性（连续内存）
    - 便于统一释放（避免碎片）

2. 为什么选择指针 bump 分配?
    - O(1) 分配速度
    - 无需维护空闲链表
    - 适合短生命周期对象

3. 为什么不用 TLS (Thread Local Storage)?
    - 教程简化（第一阶段单线程）
    - 后续可扩展为 Per-thread Arena

**核心代码结构**:
```cpp
class Arena {
  char* ptr_;                    // 当前分配位置
  char* end_;                    // 当前块结束位置
  size_t block_size_;             // 每块大小
  std::vector<void*> blocks_;     // 已分配的块

public:
  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
  void Reset();
  size_t MemoryUsage() const;
};
```

**文件清单**:
```
include/nano_redis/arena.h    # Arena 定义
tests/arena_test.cc          # Arena 测试
tests/arena_bench.cc         # 性能基准测试
docs/DESIGN.md               # Arena 设计决策
docs/ARCHITECTURE.md          # 内存布局图
docs/PERFORMANCE.md            # Arena vs malloc 性能对比
docs/LESSONS_LEARNED.md        # 内存分配知识点
```

**代码量**: ~300 行

**测试**: 分配/释放性能测试，对比标准 malloc

**性能对比**:
```
操作              | Arena  | malloc  | 提升
------------------|--------|---------|------
分配 1000 小对象   | 0.01ms | 0.10ms  | 10x
释放 (批量)       | 0.001ms| N/A    | ∞
内存碎片          | 0%      | ~15%    | -
```

---

#### **Commit 3: 数据结构选型 - flat_hash_map vs unordered_map**

**学习目标**:
- 理解 Swiss Table 的设计原理
- 学习 open addressing vs chaining
- 对比不同 hash table 的性能特征

**设计决策**:
1. 为什么选 flat_hash_map 而不是 std::unordered_map?
   - Cache locality（元素连续存储）
   - SIMD 加速的探测（Group-based）
   - 更小的内存占用（无链表指针）

2. open addressing 的权衡:
   优点:
   - 高缓存命中率
   - 空间效率高
   缺点:
   - 删除复杂（标记删除）
   - 需要处理聚集（quadratic probing）

3. 为什么不用 node_hash_map?
   - pointer stability 不是我们的需求
   - 额外的间接访问开销

**核心代码结构**:
```cpp
// 统一使用 std::string
using StringStore = absl::flat_hash_map<std::string, std::string>;

// 异构查找 - 使用 std::string 作为 key
StoredValue* Get(const std::string& key);
```

**文件清单**:
```
include/nano_redis/string_store.h    # 字符串存储
tests/hash_table_bench.cc          # 性能基准
docs/DESIGN.md                    # Swiss Table 原理
docs/ARCHITECTURE.md               # 控制字节布局
docs/PERFORMANCE.md                # flat_hash_map vs unordered_map
docs/LESSONS_LEARNED.md            # 哈希表知识点
```

**代码量**: ~350 行

**测试**: 读写性能对比，内存占用对比

**Swiss Table 原理**:
```
Control byte 结构:
- kEmpty = -128 (0x80): 空槽，MSB=1
- kDeleted = -2 (0xFE): 已删除，MSB=1
- kSentinel = -1 (0xFF): 探测终止符，MSB=1
- Full slot: H2 hash value, MSB=0

SIMD 探测:
_mm_cmpeq_epi8 一次性匹配 16 个槽
_mm_movemask_epi8 提取匹配掩码
```

**性能对比**:
```
操作        | flat_hash_map | unordered_map | 提升
------------|--------------|---------------|------
Insert 1M  | 120ms       | 280ms         | 2.3x
Lookup 1M  | 80ms        | 150ms         | 1.9x
Memory      | 64MB        | 96MB          | 1.5x
```

---

#### **Commit 4: 字符串处理 - std::string 高效使用**

**学习目标**:
- 理解 std::string 的内部实现（SSO - Small String Optimization）
- 学习如何避免不必要的字符串拷贝
- 掌握字符串操作的性能优化技巧

**设计决策**:
1. 为什么只用 std::string?
    - C++17 标准库，无需额外依赖
    - SSO 优化（小字符串栈上分配）
    - 现代编译器优化（COW -> move semantics）

2. 为什么不使用 string_view 或 Cord?
    - 简化项目复杂度（聚焦核心功能）
    - std::string 足够高效
    - 避免生命周期管理问题

3. 字符串拷贝优化技巧:
    - 使用 `std::move` 转移所有权
    - 引用传递 `const std::string&`（只读）
    - 返回值优化（RVO/NRVO）

**文件清单**:
```
include/nano_redis/string_utils.h    # 字符串工具
tests/string_bench.cc               # 字符串性能测试
docs/DESIGN.md                     # std::string 设计
docs/ARCHITECTURE.md                | SSO 内部结构
docs/PERFORMANCE.md                 | 性能优化技巧
docs/LESSONS_LEARNED.md             | 字符串知识点
```

**代码量**: ~200 行

**SSO (Small String Optimization) 示例**:
```
std::string s = "short";  // 栈上存储，无堆分配
std::string l = "a very long string that exceeds SSO threshold";  // 堆分配
```

---

#### **Commit 5: 单元测试框架 - 测试驱动开发**

**学习目标**:
- 建立 TDD 流程
- 学习如何写性能测试
- 引入基准测试框架

**设计决策**:
1. 为什么选 GoogleTest?
   - Abseil 内置支持
   - 丰富的断言宏
   - 基准测试集成

2. 为什么需要基准测试?
   - 量化性能改进
   - 回归检测（性能退化）
   - 指导优化方向

**文件清单**:
```
include/nano_redis/test_utils.h    # 测试工具类
tests/test_main.cc                  # 测试入口
docs/DESIGN.md                      # 测试策略
docs/LESSONS_LEARNED.md              # TDD 知识点
```

**代码量**: ~250 行

---

### 🟡 第二阶段：网络层和协议（提交 6-9）

---

#### **Commit 6: Socket 基础 - 同步 Echo 服务器**

**学习目标**:
- 理解 TCP socket 编程基础
- 学习事件驱动的编程模型
- 建立网络通信的基准

**设计决策**:
1. 为什么先用同步 socket?
   - 从简单开始，理解基本流程
   - 后续迁移到 io_uring 有清晰对比
   - 教学渐进性

2. 为什么用 epoll 而不是 select?
   - O(1) vs O(n) 复杂度
   - 文件描述符数量不受限
   - 更好的事件通知机制

3. 为什么用 Edge-triggered 而不是 Level-triggered?
   - 更少的系统调用（只通知状态变化）
   - 稍微复杂一些（需要完全处理事件）
   - io_uring 默认行为（为后续迁移做准备）

**核心代码结构**:
```cpp
class EpollServer {
  int epoll_fd_;
  int listen_fd_;

  void Loop();
  void HandleAccept(int fd);
  void HandleRead(int fd);
};
```

**文件清单**:
```
include/nano_redis/epoll_server.h    # epoll 服务器
src/epoll_server.cc                  # epoll 实现
tests/echo_server_test.cc             # echo 测试
docs/DESIGN.md                        # epoll vs select vs poll
docs/ARCHITECTURE.md                   # 事件循环流程图
docs/PERFORMANCE.md                    # 并发性能测试
docs/LESSONS_LEARNED.md                socket 编程知识点
```

**代码量**: ~300 行

**事件循环流程**:
```
事件循环:
while (running) {
  epoll_wait(events)  // 等待事件

  for each event:
    if (event is new connection):
      accept() + add to epoll
    if (event is readable):
      read() + process() + write()
}
```

---

#### **Commit 7: 协议解析 - RESP (REdis Serialization Protocol)**

**学习目标**:
- 理解 RESP 协议设计
- 学习流式解析器设计
- 掌握零拷贝解析技巧

**设计决策**:
1. 为什么用 RESP 而不是自定义协议?
   - 标准化（易于互操作）
   - 简单高效（易于实现）
   - 人性化（可读，便于调试）

2. 为什么设计为流式解析器?
   - 支持大数据（不需要一次性读入全部）
   - 内存高效（只保留必要的数据）
   - 符合 Redis 的行为

3. 为什么用 string_view 作为返回类型?
   - 零拷贝（避免字符串拷贝）
   - 调用者决定是否需要拷贝
   - 减少内存分配

**核心代码结构**:
```cpp
class RespParser {
  absl::string_view remaining_;

public:
  std::optional<RespValue> ParseOne();

private:
  RespValue ParseSimpleString();   // +OK\r\n
  RespValue ParseError();          // -Error\r\n
  RespValue ParseInteger();        // :123\r\n
  RespValue ParseBulkString();     // $6\r\nfoobar\r\n
  RespValue ParseArray();         // *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
};
```

**文件清单**:
```
include/nano_redis/resp_parser.h     # RESP 解析器
include/nano_redis/resp_types.h       # RESP 数据类型
tests/resp_parser_test.cc            # RESP 解析测试
docs/DESIGN.md                         # RESP 协议设计
docs/ARCHITECTURE.md                    # 解析器状态机
docs/LESSONS_LEARNED.md                  # 协议设计知识点
```

**代码量**: ~350 行

**RESP 类型映射**:
```cpp
enum class RespType {
  SimpleString = '+',  // +OK\r\n
  Error = '-',         // -Error message\r\n
  Integer = ':',       // :123\r\n
  BulkString = '$',    // $6\r\nfoobar\r\n
  Array = '*'          // *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
};
```

---

#### **Commit 8: 命令注册和路由 - 命令模式**

**学习目标**:
- 理解命令模式（Command Pattern）
- 学习如何设计可扩展的命令系统
- 掌握函数对象和 lambda 的使用
- 理解自实现 task<T> 异步机制

**设计决策**:
1. 为什么用命令注册表而不是 if-else?
    - O(1) 查找时间
    - 易于扩展（添加新命令不需要改核心逻辑）
    - 支持动态注册

2. 为什么用 flat_hash_map 存储命令?
    - 命令数量少（查找不频繁）
    - 编译时字符串作为 key
    - 与数据存储一致的体验

 3. 为什么用自实现 task<T> 而不是 C++20 协程?
     - C++17 兼容性更好
     - 自实现有助于理解协程原理
     - 可控的执行模型（非抢占式）
     - 与 io_uring 配合更灵活

**task<T> 详细设计**:

基于状态机的轻量级协程实现：
- **Awaitable 接口**：`await_ready()`, `await_suspend()`, `await_resume()`
- **Promise 状态**：存储中间结果和执行状态
- **执行器模式**：与 io_uring 配合的事件驱动执行

简化实现（教学版本）：
```cpp
// 状态机状态
enum class TaskState {
  kPending,
  kReady,
  kDone
};

template<typename T>
class Task {
  struct State {
    TaskState state = TaskState::kPending;
    T value;
    std::exception_ptr exception;
  };

  std::shared_ptr<State> state_;

public:
  // 简化的 awaitable 接口
  bool await_ready() const { return state_->state != TaskState::kPending; }
  void await_suspend(std::coroutine_handle<> handle);
  T await_resume() { return std::move(state_->value); }
};
```

**核心代码结构**:
```cpp
// 自实现的轻量级 task<T>
template<typename T>
class Task {
  using CoroHandle = void*;  // 简化的协程句柄
  std::unique_ptr<PromiseState> state_;

public:
  struct promise_type;
  Task(promise_type* p);
  bool await_ready();
  void await_suspend(std::experimental::coroutine_handle<>);
  T await_resume();
};

using CommandHandler = Task<RespValue>(Database& db, const std::vector<RespValue>& args);

class CommandRegistry {
  absl::flat_hash_map<std::string, std::function<Task<RespValue>(Database&, const std::vector<RespValue>&)>> commands_;

public:
  void Register(const std::string& name, CommandHandler handler);
  std::function<Task<RespValue>(Database&, const std::vector<RespValue>&)>* Get(const std::string& name);
};
```

**文件清单**:
```
include/nano_redis/command_registry.h    # 命令注册表
include/nano_redis/command.h              # 命令定义
src/command_registry.cc                  # 注册表实现
tests/command_registry_test.cc            # 命令路由测试
docs/DESIGN.md                              # 命令模式设计
docs/ARCHITECTURE.md                         # 命令流程图
docs/LESSONS_LEARNED.md                   # 设计模式知识点
```

**代码量**: ~280 行

---

#### **Commit 9: 命令实现 - String 类型基础操作**

**学习目标**:
- 实现基础的 Redis 命令
- 学习错误处理和响应格式化
- 掌握数据库抽象层设计

**设计决策**:
1. 为什么要抽象 Database 类?
   - 解耦数据存储和协议处理
   - 便于后续添加多数据库支持
   - 测试友好（可以 mock）

2. 为什么用 flat_hash_map<string, StoredValue>?
   - string 作为 key（需要所有权）
   - StoredValue 包含 value + 元数据（TTL）
   - 直接映射 Redis 的内存模型

 3. 为什么用 std::string 存储值?
    - C++17 标准库，无额外依赖
    - SSO 优化（小值自动 inline）
    - move semantics 避免不必要的拷贝
    - 足够高效，满足教学需求

**核心代码结构**:
```cpp
class Database {
  struct StoredValue {
    std::string value;  // 改用 std::string（C++17）
    std::chrono::steady_clock::time_point expiry;

    bool is_expired() const;
  };

  absl::flat_hash_map<std::string, StoredValue> store_;

public:
  Task<RespValue> Get(const std::string& key);
  Task<RespValue> Set(const std::string& key, const std::string& value, std::chrono::seconds ttl);
  Task<RespValue> Del(const std::vector<std::string>& keys);
  Task<RespValue> Exists(const std::vector<std::string>& keys);
};
```

**文件清单**:
```
include/nano_redis/database.h           # 数据库抽象
include/nano_redis/string_store.h      # String 存储
src/database.cc                          # 数据库实现
src/string_commands.cc                    # String 命令
tests/string_commands_test.cc              # String 命令测试
docs/DESIGN.md                                # 数据库抽象设计
docs/ARCHITECTURE.md                           # 数据流图
docs/LESSONS_LEARNED.md                     # 数据库设计知识点
```

**代码量**: ~320 行

---

### 🔵 第三阶段：数据类型扩展（提交 10-14）

---

#### **Commit 10: Hash 类型 - 嵌套 flat_hash_map**

**学习目标**:
- 理解嵌套数据结构的设计
- 学习如何高效实现 HGETALL
- 掌握内存布局对性能的影响

**设计决策**:
1. 为什么用 flat_hash_map<key, flat_hash_map<field, value>>?
   - 直接映射 Redis Hash 的语义
   - 字段级别的快速查找 O(1)
   - 内存连续性好（两个层级的 Swiss Table）

2. 为什么不单独存储 Hash 对象?
   - 避免额外的一层间接访问
   - 利用 flat_hash_map 的内部优化
   - 简化代码

3. HGETALL 为什么需要临时构建结果?
   - RESP 数组需要一次性序列化
   - 避免 lock 持有时间过长
   - 便于后续添加分页

**核心代码结构**:
```cpp
class HashStore {
  using FieldMap = absl::flat_hash_map<std::string, std::string>;
  absl::flat_hash_map<std::string, FieldMap> hash_store_;

public:
  Task<RespValue> HSet(const std::string& key, const std::string& field, const std::string& value);
  Task<RespValue> HGet(const std::string& key, const std::string& field);
  Task<RespValue> HDel(const std::string& key, const std::vector<std::string>& fields);
  Task<RespValue> HGetAll(const std::string& key);  // 返回 {field, value} 数组
};
```

**文件清单**:
```
include/nano_redis/hash_store.h        # Hash 存储
src/hash_commands.cc                   # Hash 命令
tests/hash_commands_test.cc             # Hash 命令测试
docs/DESIGN.md                              # 嵌套结构设计
docs/ARCHITECTURE.md                         # 内存布局图
docs/LESSONS_LEARNED.md                     # 嵌套容器知识点
```

**代码量**: ~280 行

---

#### **Commit 11: List 类型 - InlinedVector 实践**

**学习目标**:
- 理解 InlinedVector 的适用场景
- 学习双端队列的实现
- 掌握 LRANGE 的分页处理

**设计决策**:
1. 为什么用 InlinedVector 而不是 std::list?
   - 连续内存（缓存友好）
   - 小列表无堆分配（inline 存储）
   - 随机访问 O(1) 而不是 O(n)

2. 为什么 inline size 设为 8?
   - 覆盖常见场景（小列表）
   - 平衡内存和性能
   - 一个缓存行（64 bytes）可容纳多个元素

3. LRANGE 如何处理超大 range?
   - 先转换为索引
   - 迭代器直接访问（无拷贝）
   - 限制最大返回数量（防止 OOM）

**核心代码结构**:
```cpp
class ListStore {
  using ListType = absl::InlinedVector<std::string, 8>;
  absl::flat_hash_map<std::string, ListType> list_store_;

public:
  Task<RespValue> LPush(const std::string& key, const std::vector<std::string>& values);
  Task<RespValue> RPush(const std::string& key, const std::vector<std::string>& values);
  Task<RespValue> LPop(const std::string& key, size_t count = 1);
  Task<RespValue> RPop(const std::string& key, size_t count = 1);
  Task<RespValue> LRange(const std::string& key, int64_t start, int64_t stop);
};
```

**文件清单**:
```
include/nano_redis/list_store.h         # List 存储
src/list_commands.cc                   # List 命令
tests/list_commands_test.cc             # List 命令测试
docs/DESIGN.md                              # InlinedVector 设计
docs/ARCHITECTURE.md                         # 内存布局图
docs/LESSONS_LEARNED.md                     # 向量优化知识点
```

**代码量**: ~320 行

**内存布局图**:
```
Small List (≤8 elements):         Large List (>8 elements):
[metadata][inline element 0...7]    [metadata][ptr to heap]
                                  [heap: elements...]
```

---

#### **Commit 12: Set 类型 - flat_hash_set**

**学习目标**:
- 理解 Set 和 Map 的区别
- 学习集合运算的实现
- 掌握 SINTER/SUNION 的优化

**设计决策**:
1. 为什么用 flat_hash_set 而不是 flat_hash_map?
   - Set 只需要键，不需要值
   - 节省 50% 内存（无 value）
   - 更快的插入（不需要存储 value）

2. 集合运算为什么需要临时容器?
   - 避免修改原集合
   - 支持链式操作（SINTER key1 key2 key3）
   - 便于实现交集优化（从小到大）

 3. 为什么用 std::string 作为集合元素?
    - C++17 标准库，无额外依赖
    - SSO 优化（短字符串无堆分配）
    - move semantics 传递高效
    - 与其他数据类型保持一致

**核心代码结构**:
```cpp
class SetStore {
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> set_store_;

public:
  Task<RespValue> SAdd(const std::string& key, const std::vector<std::string>& members);
  Task<RespValue> SRem(const std::string& key, const std::vector<std::string>& members);
  Task<RespValue> SMembers(const std::string& key);
  Task<RespValue> SIsMember(const std::string& key, const std::string& member);
  Task<RespValue> SInter(const std::vector<std::string>& keys);
  Task<RespValue> SUnion(const std::vector<std::string>& keys);
};
```

**文件清单**:
```
include/nano_redis/set_store.h          # Set 存储
src/set_commands.cc                    # Set 命令
tests/set_commands_test.cc              # Set 命令测试
docs/DESIGN.md                              # Set vs Map 对比
docs/ARCHITECTURE.md                         # 集合运算流程
docs/LESSONS_LEARNED.md                     # 集合知识点
```

**代码量**: ~260 行

---

#### **Commit 13: 过期管理 - Time Wheel 算法**

**学习目标**:
- 理解 Time Wheel 的时间复杂度优势
- 学习如何高效管理大量定时器
- 掌握惰性删除策略

**设计决策**:
1. 为什么用 Time Wheel 而不是 sorted map?
    - O(1) 插入/删除 vs O(log n)
    - 无需复杂的树结构
    - 适合大量短命名的 key

2. 为什么轮大小选 1024?
    - 平衡内存和精度
    - Tick 间隔 10ms，总覆盖 10.24s
    - 更长的 TTL 使用多级轮

3. 为什么用惰性删除?
    - 避免阻塞请求
    - 访问时检查过期
    - 定期批量清理（后台线程）

**核心代码结构**:
```cpp
class TimeWheel {
  static constexpr size_t kWheelSize = 1024;
  static constexpr std::chrono::milliseconds kTickDuration{10};

  struct Bucket {
    std::vector<std::pair<std::string, int64_t>> entries;
  };

  std::array<Bucket, kWheelSize> wheel_;
  size_t current_tick_ = 0;

public:
  void Add(const std::string& key, std::chrono::milliseconds ttl);
  void Tick();  // 每个 tick 周期调用
};

class Database {
  TimeWheel expire_wheel_;
  std::mutex expire_mutex_;  // 保护 wheel 和 store 的并发访问

  Task<RespValue> Expire(const std::string& key, std::chrono::seconds ttl);
  Task<RespValue> TTL(const std::string& key);
};
```

**文件清单**:
```
include/nano_redis/time_wheel.h       # 时间轮
include/nano_redis/expire_manager.h   # 过期管理器
src/expire_commands.cc                   # 过期命令
tests/expire_test.cc                    # 过期测试
docs/DESIGN.md                                # Time Wheel 原理
docs/ARCHITECTURE.md                           # 时间轮图解
docs/PERFORMANCE.md                            # O(1) vs O(log n) 对比
docs/LESSONS_LEARNED.md                     # 定时器算法知识点
```

**代码量**: ~340 行

**Time Wheel 原理图**:
```
      0ms     10ms    20ms    ...   10230ms 10240ms
       |       |       |              |       |
       v       v       v              v       v
    ┌────┬────┬────┬────┬────┬────┬────┐
    │bucket│bucket│bucket│ ... │bucket│bucket│
    └────┴────┴────┴────┴────┴────┴────┘
       ^
    current_tick

插入 TTL=50ms 的 key:
    bucket[(current_tick_ + 50/10) % 1024]
```

---

#### **Commit 14: 完整命令集 - 兼容性测试**

**学习目标**:
- 实现完整的 Redis 子集命令
- 学习如何做兼容性测试
- 建立性能基准

**设计决策**:
1. 为什么只实现子集命令?
   - 控制代码量（<5000 行）
   - 聚焦核心功能
   - 便于学习和测试

2. 命令选择原则:
   - String: GET/SET/DEL/EXISTS/INCR/DECR/EXPIRE/TTL
   - Hash: HSET/HGET/HDEL/HGETALL/HEXISTS
   - List: LPUSH/RPUSH/LPOP/RPOP/LRANGE/LLEN
   - Set: SADD/SREM/SMEMBERS/SINTER/SUNION/SISMEMBER

3. 为什么用 redis-benchmark 作为测试工具?
   - 标准化测试
   - 可量化的性能指标
   - 易于与官方 Redis 对比

**文件清单**:
```
include/nano_redis/commands.h           # 所有命令定义
src/commands.cc                          # 命令注册
tests/commands_compatibility_test.cc      # 兼容性测试
docs/COMMANDS.md                           # 命令参考
docs/PERFORMANCE.md                        # 性能基准
```

**代码量**: ~250 行

**命令总览表**:
```
Type | Commands Implemented | Commands Skipped
------|----------------------|------------------
String | 9/50 | APPEND/GETSET/MSET/MGET/...
Hash   | 5/20 | HMGET/HKEYS/HVALS/HINCRBY/...
List   | 6/30 | LINDEX/LINSERT/LREM/LSET/...
Set    | 6/15 | SPOP/SRANDMEMBER/SMOVE/...
```

---

### 🔴 第四阶段：性能优化和高级特性（提交 15-18）

---

#### **Commit 15: 性能分析 - 瓶颈识别和优化**

**学习目标**:
- 使用 perf 工具分析性能
- 识别热点代码路径
- 学习常见优化技巧

**设计决策**:
1. 为什么用 perf 而不是 profiler?
   - Linux 原生工具
   - 无需修改代码
   - 火焰图可视化

2. 常见瓶颈及优化:
   - 内存分配 → 使用 Arena
   - 字符串拷贝 → 零拷贝 string_view
   - 系统调用 → io_uring（下一阶段）
   - 缓存 miss → 数据布局优化

3. 基准测试优化前后对比:
   - QPS 提升幅度
   - Latency 分布
   - CPU 使用率

**文件清单**:
```
tests/benchmarks.cc                     # 性能基准
scripts/analyze_perf.sh                  # perf 分析脚本
docs/PERFORMANCE.md                          # 性能分析报告
docs/LESSONS_LEARNED.md                    # 性能优化知识点
```

**代码量**: ~150 行（主要是优化，新增代码少）

**优化前后对比**:
```
操作        | 优化前 QPS | 优化后 QPS | 提升
------------|-----------|-----------|------
SET         | 50K       | 150K      | 3x
GET         | 80K       | 250K      | 3.1x
LPUSH       | 30K       | 100K      | 3.3x
LRANGE(100) | 10K       | 40K       | 4x
```

**perf 使用示例**:
```bash
# CPU 火焰图
perf record -g ./nano_redis
perf report

# 缓存 miss 分析
perf stat -e cache-references,cache-misses ./nano_redis

# 系统调用统计
perf stat -e syscalls:sys_enter_getpid,syscalls:sys_enter_read ...
```

---

#### **Commit 16: io_uring 迁移 - 第一部分（Read/Write）**

**学习目标**:
- 理解 io_uring 的核心概念
- 学习 SQE/CQE 提交/获取机制
- 掌握零拷贝 I/O

**设计决策**:
1. 为什么先迁移 Read/Write?
   - 这是核心操作
   - 其他操作基于此构建
   - 降低迁移风险

2. 为什么保留 epoll 作为 fallback?
   - 兼容旧内核
   - 对比性能差异
   - 教学目的

 3. io_uring 参数调优:
    - entries=4096（平衡内存和并发）
    - SQ_POLL（减少 syscall）
    - FAST_POLL（epoll 加速）

**迁移策略**:

从 epoll 到 io_uring 的渐进式迁移：
- **阶段 1**：实现 IoUring 封装类，与 EpollServer 并存
- **阶段 2**：抽象 I/O 接口，支持两种后端切换
- **阶段 3**：逐步将 Read/Write 操作迁移到 io_uring
- **阶段 4**：迁移 Accept/Close 操作，使用链接优化
- **阶段 5**：完全移除 epoll 代码（保留为 fallback 选项）

接口抽象设计：
```cpp
// 通用 I/O 接口
class AsyncIo {
public:
  virtual ~AsyncIo() = default;
  virtual task<ssize_t> Read(int fd, void* buf, size_t len) = 0;
  virtual task<ssize_t> Write(int fd, const void* buf, size_t len) = 0;
  virtual task<int> Accept(int listen_fd, sockaddr* addr, socklen_t* len) = 0;
};

// epoll 实现
class EpollIo : public AsyncIo { /* ... */ };

// io_uring 实现
class IoUringIo : public AsyncIo { /* ... */ };
```

**核心代码结构**:
```cpp
class IoUring {
  struct io_uring ring_;

public:
  IoUring(size_t entries = 4096);
  ~IoUring();

  // 异步读取
  task<ssize_t> Read(int fd, void* buf, size_t len);

  // 异步写入
  task<ssize_t> Write(int fd, const void* buf, size_t len);

  // 等待完成事件
  void WaitCqe(io_uring_cqe** cqe);
};
```

**文件清单**:
```
include/nano_redis/io_uring.h            # io_uring 封装
include/nano_redis/async_socket.h        # 异步 socket
src/io_uring.cc                            # io_uring 实现
src/async_socket.cc                        # 异步 socket 实现
tests/io_uring_test.cc                     # io_uring 测试
tests/async_socket_test.cc                 # 异步 socket 测试
docs/DESIGN.md                                    # io_uring 设计
docs/ARCHITECTURE.md                               # SQ/CQ 结构
docs/PERFORMANCE.md                                # epoll vs io_uring 性能
docs/LESSONS_LEARNED.md                          # 异步 I/O 知识点
```

**代码量**: ~400 行

**性能对比**:
```
场景        | epoll QPS | io_uring QPS | 提升
------------|----------|-------------|------
Small reads | 80K      | 250K       | 3.1x
Large reads | 20K      | 120K       | 6x
Mixed       | 50K      | 180K       | 3.6x
```

---

#### **Commit 17: io_uring 迁移 - 第二部分（Accept/Close/批量）**

**学习目标**:
- 完整迁移到 io_uring
- 学习批量提交优化
- 掌握链接操作

**设计决策**:
1. 为什么链接 Accept 和 Read?
   - 减少 syscall
   - 降低延迟
   - 连接立即准备好读取

2. 为什么用批量提交?
   - 单次 submit 处理多个操作
   - 提升 CPU 缓存利用率
   - 减少 syscall 开销

3. 链接操作示例:
   ```
   IORING_OP_LINK → 依赖关系
   Accept → Read(后续)
   ```

**核心代码结构**:
```cpp
// Accept -> Read 链接
io_uring_sqe* sqe_accept = io_uring_get_sqe(&ring_);
io_uring_prep_accept(sqe_accept, listen_fd, ...);

io_uring_sqe* sqe_read = io_uring_get_sqe(&ring_);
io_uring_prep_read(sqe_read, client_fd, buf, ...);
io_uring_sqe_set_flags(sqe_read, IOSQE_IO_LINK);  // 依赖前一个 SQE

io_uring_submit(&ring_);  // 一次提交两个操作
```

**文件清单**:
```
include/nano_redis/io_uring_batch.h    # 批量提交
src/io_uring_server.cc                    # io_uring 服务器
tests/io_uring_server_test.cc            # io_uring 服务器测试
docs/DESIGN.md                                # 批量提交优化
docs/ARCHITECTURE.md                           | 链接操作图
docs/PERFORMANCE.md                            | 批量 vs 单次提交
docs/LESSONS_LEARNED.md                     | io_uring 高级特性
```

**代码量**: ~350 行

**批量提交优化**:
```cpp
class SubmissionBatcher {
  std::vector<io_uring_sqe*> pending_;
  size_t threshold_ = 64;

public:
  template<typename SetupFunc>
  void Add(SetupFunc&& setup) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    setup(sqe);
    pending_.push_back(sqe);

    if (pending_.size() >= threshold_) {
      Flush();
    }
  }

  void Flush() {
    if (!pending_.empty()) {
      io_uring_submit(&ring_);
      pending_.clear();
    }
  }
};
```

---

#### **Commit 18: 总结和展望 - 完整的 nano-redis**

**学习目标**:
- 总结整个项目的技术点
- 建立性能基准
- 规划后续优化方向

**设计决策**:
1. 性能总结:
   - QPS vs 官方 Redis 对比
   - 内存占用对比
   - 代码复杂度对比

2. 技术亮点总结:
   - io_uring + 协程的异步模型
   - Abseil 容器的高效使用
   - Time Wheel 的 O(1) 过期管理
   - 零拷贝的设计哲学

3. 后续优化方向:
   - 多线程模型（后台过期清理）
   - RDB/AOF 持久化
   - 集群支持（主从复制）
   - 更多的数据类型（Sorted Set, Stream）

**文件清单**:
```
include/nano_redis/redis_server.h     # 主服务器
src/redis_server.cc                      # 服务器实现
src/main.cc                               # 入口点
tests/full_integration_test.cc            # 完整集成测试
docs/PERFORMANCE.md                          | 最终性能报告
docs/ROADMAP.md                             | 后续路线图
README.md                                   | 项目说明（更新）
```

**代码量**: ~100 行（文档和基准测试）

**最终性能表**:
```
指标               | Nano-Redis | Redis | 比例
------------------|-----------|--------|------
代码行数            | 4800      | 100K+  | 5%+
QPS (GET)          | 250K      | 300K   | 83%
QPS (SET)          | 150K      | 200K   | 75%
内存占用 (1M keys) | 200MB     | 220MB  | 91%
启动时间           | 10ms      | 50ms   | 5x
```

**技术栈总结**:
```
┌─────────────────────────────────────────────────┐
│           Nano-Redis 架构概览                │
├─────────────────────────────────────────────────┤
│ 网络层: io_uring (零拷贝，批量提交)         │
│ 异步模型: 自实现 Task<T> (C++17)            │
│ 协议: RESP2 (高效解析)                     │
├─────────────────────────────────────────────────┤
│ 存储: flat_hash_map (Swiss Table)            │
│ List: InlinedVector (inline 优化)           │
│ String: std::string (SSO 优化)              │
│ 过期: Time Wheel (O(1))                   │
├─────────────────────────────────────────────────┤
│ 内存: Arena Allocator (批量分配)            │
│ 同步: std::mutex                          │
│ 时间: std::chrono (标准库)                │
│ 构建: CMake                                │
└─────────────────────────────────────────────────┘
```

---

## 📚 教学文档结构

每个提交都包含以下文件:

```
commit_XX_<feature>/
├── include/nano_redis/           # 新增头文件
├── src/                       # 新增实现文件
├── tests/                     # 单元测试
│   ├── <feature>_test.cc
│   └── <feature>_bench.cc
└── docs/                      # 教学文档
    ├── DESIGN.md              # 设计决策说明
    ├── ARCHITECTURE.md        # 架构图
    ├── PERFORMANCE.md          # 性能分析
    └── LESSONS_LEARNED.md    # 学习要点
```

**示例: docs/DESIGN.md 格式**:
```markdown
# <Feature> 设计决策

## 为什么使用 <Feature>?

### 问题
<描述问题>

### 解决方案
<描述解决方案>

## 设计权衡

| 方案 | 优点 | 缺点 |
|------|------|------|
| ...  | ...  | ... |

## 适用场景

✅ 适用:
- ...
- ...

❌ 不适用:
- ...
- ...
```

---

## 🎯 验收标准

每个提交必须满足:

1. **代码质量**:
   - [x] 遵循 Google C++ Style Guide
   - [x] 通过 clang-format 检查
   - [x] 包含完整的注释

2. **测试覆盖**:
   - [x] 单元测试通过
   - [x] 性能基准建立
   - [x] 边界情况处理

3. **文档完整性**:
   - [x] DESIGN.md (设计决策)
   - [x] ARCHITECTURE.md (架构图)
   - [x] PERFORMANCE.md (性能数据)
   - [x] README.md 更新

4. **Git 提交信息**:
   ```
   commit XX: [Feature] <feature description>

   - What: 实现了什么功能
   - Why: 为什么要这样设计
   - How: 关键技术点
   - Perf: 性能提升/基准
   ```

---

## 📊 代码量统计（总览）

```
阶段              | 提交数 | 新增代码 | 累计代码
------------------|---------|----------|----------
第一阶段: 基础设施 | 5       | ~1280   | ~1280
第二阶段: 网络层   | 4       | ~1250   | ~2530
第三阶段: 数据类型 | 5       | ~1450   | ~3980
第四阶段: 性能优化 | 4       | ~1000   | ~4980
------------------|---------|----------|----------
总计              | 18      | ~4980   | ~4980
```

---

## 🚀 开始实施

### 状态更新

- ✅ **Commit 01**: Hello World - 项目脚手架（已完成）
- ⏳ **Commit 02**: Arena Allocator（待实施）
- ⏳ **Commit 03**: flat_hash_map vs unordered_map（待实施）
- ... (后续提交待实施）

### 下一步

执行 `Commit 02: Arena Allocator`：
```bash
cd /home/ubuntu/nano_redis
# 开始实施 Arena Allocator...
```

---

## 🎓 学习资源

### 在线资源

| 主题 | 资源 | 链接 |
|------|------|------|
| **C++17** | cppreference | https://en.cppreference.com/w/cpp/17 |
| **io_uring** | liburing 文档 | https://unixism.net/loti/ |
| **Abseil** | Abseil Guide | https://abseil.io/docs/cpp/ |
| **Swiss Tables** | Abseil Blog | https://abseil.io/blog/ |
| **Google C++ Style** | Style Guide | https://google.github.io/styleguide/cppguide.html |

### 推荐阅读

1. **《C++ 并发编程实战》** - 学习并发基础
2. **《Linux 高性能服务器编程》** - 深入系统编程
3. **《Redis 设计与实现》** - 理解 Redis 原理
4. **《CPU 缓存优化》** - 学习缓存友好设计

---

## ❓ 常见问题

### Q1: 为什么代码量限制在 5000 行？
**A**: 聚焦核心功能，便于教学和学习。完整 Redis 约 10 万行代码。

### Q2: 可以跳过某些提交吗？
**A**: 可以，但建议按顺序学习，因为后续依赖前面的基础。

### Q3: 如何构建和运行？
**A**:
```bash
# CMake
mkdir build && cd build
cmake ..
cmake --build .
./tests/xxx_test

# Bazel
bazel build //...
bazel test //tests/xxx_test
```

### Q4: 需要什么环境？
**A**:
- Linux 5.1+ (io_uring 支持)
- C++17 编译器 (GCC 8+, Clang 7+)
- CMake 3.16+
- GoogleTest
- Abseil-Cpp (使用 add_subdirectory 引入)

---

**📚 完整方案已保存到 PROJECT_PLAN.md！准备好开始 Commit 02 了吗？**
