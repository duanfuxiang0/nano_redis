# nano_redis 渐进式并发重构路径

## 概述

基于核心假设验证结果，设计一个更实用的、渐进式的重构路径，避免大规模重写。

---

## 🔴 紧急风险：Database 全局指针问题

### ⚠️ 严重性：🔴 致命 - 必须在开始实施前修复！

---

### 问题描述

当前命令实现（StringFamily、HashFamily、SetFamily、ListFamily）使用全局静态 Database 指针，这会在多 EngineShard 环境下导致灾难性的数据竞争。

#### 1. StringFamily 中的问题
```cpp
// src/command/string_family.cc:10-20
namespace {
    Database* g_database = nullptr;  // ❌ 全局静态指针

    Database* GetDatabase() {
        if (g_database) {
            return g_database;  // ❌ 所有线程返回同一个指针
        }
        static thread_local Database database;
        return &database;
    }
}

void StringFamily::SetDatabase(Database* db) {
    g_database = db;  // ❌ 设置全局指针
}
```

#### 2. HashFamily 中的问题（更严重）
```cpp
// src/command/hash_family.cc:7-12
static Database* g_db = nullptr;  // ❌ 全局静态指针

namespace {
    Database* GetDatabase() {
        return g_db;  // ❌ 没有任何 thread_local fallback！
    }
}

void HashFamily::SetDatabase(Database* db) {
    g_db = db;  // ❌ 直接使用全局指针
}
```

### 灾难场景

假设启动 4 个 EngineShard：

```cpp
// EngineShard 0 启动
StringFamily::SetDatabase(&shard0_db_);
// 结果：g_database = &shard0_db_

// EngineShard 1 启动
StringFamily::SetDatabase(&shard1_db_);
// 结果：g_database = &shard1_db_  ❌ 覆盖了 shard0！

// EngineShard 2 启动
StringFamily::SetDatabase(&shard2_db_);
// 结果：g_database = &shard2_db_  ❌ 覆盖了 shard1！

// EngineShard 3 启动
StringFamily::SetDatabase(&shard3_db_);
// 结果：g_database = &shard3_db_  ❌ 覆盖了 shard2！
```

**后果：**
- ❌ **所有 EngineShard 的线程都在访问同一个 `g_database`**
- ❌ **多个线程并发读写同一个 Database 实例**
- ❌ **完全破坏了共享无状态架构**
- ❌ **严重的 Data Race，数据随时损坏**
- ❌ **根本无法在多 EngineShard 下运行**

---

### ✅ 修复方案

**核心原则：通过 CommandContext 传递 Database，不要使用全局指针。**

#### 方案 A：修改命令签名（推荐）

```cpp
// include/core/command_context.h
#pragma once

class EngineShard;  // 前向声明

struct CommandContext {
    EngineShard* local_shard = nullptr;
    size_t shard_count = 1;
    size_t db_index = 0;
    void* connection = nullptr;

    // 便捷方法：获取当前 Database
    Database& GetDB() {
        return local_shard->GetDB();
    }
};
```

#### 方案 B：EngineShard 直接控制 DashTable（避免 Database 类）

```cpp
// include/server/engine_shard.h
class EngineShard {
public:
    static const size_t kNumDBs = 16;
    using DbIndex = size_t;

    EngineShard(size_t shard_id, uint16_t port, EngineShardSet* shard_set);
    ~EngineShard();

    // 直接存储 DashTable，不使用 Database 类
    std::array<std::unique_ptr<DashTable<CompactObj, CompactObj>>, kNumDBs> db_tables_;
    DbIndex current_db_ = 0;

    // Database-like 接口
    DbIndex CurrentDB() const { return current_db_; }
    void SelectDB(DbIndex idx) { current_db_ = idx; }

    // 直接访问 DashTable
    DashTable<CompactObj, CompactObj>& GetDBTable(DbIndex idx) {
        return *db_tables_[idx];
    }

    size_t shard_id() const { return shard_id_; }

    TaskQueue* GetTaskQueue() { return &task_queue_; }

    void Start();
    void Stop();
    void Join();

private:
    void EventLoop();

    DbIndex shard_id_;
    TaskQueue task_queue_;
    std::thread thread_;
    std::atomic<bool> running_;
    static __thread EngineShard* tlocal_shard_;
};
```

---

### 📋 需要修改的文件清单

#### 1. 删除全局 Database 指针
```bash
# 需要删除的代码
- src/command/string_family.cc:10-20  # namespace { Database* g_database; ... }
- src/command/hash_family.cc:7-17      # static Database* g_db; ... }
- src/command/set_family.cc
- src/command/list_family.cc
```

#### 2. 删除 SetDatabase 方法
```bash
# 需要删除的方法声明和实现
- include/command/string_family.h:13  # static void SetDatabase(Database* db);
- include/command/hash_family.h:13
- include/command/set_family.h:13
- include/command/list_family.h:13
```

#### 3. 删除 GetDatabase 方法
```bash
# 需要删除的方法
- include/command/string_family.h
- include/command/hash_family.h
- include/command/set_family.h
- include/command/list_family.h
```

#### 4. 修改所有命令签名
```bash
# 需要添加的参数
- 所有命令函数都需要添加 CommandContext* ctx 参数
- 或者修改 CommandRegistry 直接传递
```

---

### 📊 影响评估

| 问题 | 严重性 | 影响 | 修复工作量 |
|------|---------|------|-----------|
| 全局 g_database 指针 | 🔴 **致命** | 多线程数据竞争 | 2-3 工作日 |
| 全局 g_db 指针 | 🔴 **致命** | 无保护并发访问 | 2-3 工作日 |
| SetDatabase 覆盖 | 🔴 **致命** | 所有 shard 争夺同一指针 | 0.5-1 工作日 |
| 删除所有相关代码 | 🟡 中等 | 大量代码修改 | 3-4 工作日 |

**总计修复时间：** 5-7 工作日

---

### ✅ 测试策略

1. **Thread Sanitizer (TSAN)**
   ```bash
   # 编译时启用线程安全检测
   cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" -B build_tsan .
   cmake --build build_tsan --target unit_tests
   ./build_tsan/unit_tests
   ```

2. **并发压力测试**
   ```bash
   # 测试多线程并发读写
   for i in {1..10}; do
       ./build/unit_tests --gtest_filter=ConcurrencyTest* &
   done
   ```

3. **Valgrind 检测**
   ```bash
   valgrind --tool=helgrind --leak-check=full ./build/unit_tests
   ```

---

### 🚨 立即行动项

**在开始任何实施之前：**

1. [ ] ✅ **审查所有 command family 的 Database 使用**
2. [ ] ✅ **确认全局指针问题范围**
3. [ ] ✅ **创建详细修复计划并评审**
4. [ ] ✅ **先修复 Database 问题，再进行阶段 2**
5. [ ] ✅ **使用 TSAN 验证修复正确性**
6. [ ] ✅ **更新所有文档，说明架构变更**
7. [ ] ✅ **将 Database 全局指针修复作为**阶段 0.5**，必须在阶段 1 之前完成！

---

**警告：** 如果不修复这个问题直接开始阶段 2，会导致：
- 🔴 数据损坏和崩溃
- 🔴 无法通过任何并发测试
- 🔴 完全无法实现多 EngineShard 架构
- 🔴 所有工作浪费

**建议：** 将 Database 全局指针修复作为**阶段 0.5**，必须在阶段 1 之前完成！

---

## 关键发现

### ✅ 已验证的核心假设

1. **SO_REUSEPORT 在 photonlibos 中完全支持**
    - 官方示例：`examples/perf/multi-conn-perf.cpp`
    - 每个 OS 线程创建独立 socket
    - 所有 socket 绑定同一端口
    - 内核哈希分发连接

2. **photon::semaphore 可用于跨线程同步**
    - 官方示例：`include/photon/thread/awaiter.h`
    - 支持 `signal()` 和 `wait()` 方法
    - 可以用于实现 TaskQueue 的 Await 机制

3. **Dragonfly 的架构不是唯一正确方案**
    - Dragonfly 使用 ProactorPool + AcceptServer 模式
    - 但这不意味着 SO_REUSEPORT 方案不可行
    - 我们应该选择适合 photonlibos 的方案

---

## 渐进式重构路径（6 阶段）

### 阶段 0.5：修复 Database 全局指针问题（2-3 天）⚠️ **新增**

**目标：** 删除全局 Database 指针，修复多线程数据竞争问题

**任务：**

#### 0.5.1 审查所有命令实现
```bash
# 检查所有 command family 的 Database 使用模式
- StringFamily: g_database + SetDatabase() + thread_local fallback
- HashFamily: g_db + SetDatabase() + 无 fallback
- SetFamily: (待检查)
- ListFamily: (待检查)
```

#### 0.5.2 删除所有全局 Database 指针
```bash
# StringFamily
- 删除 namespace 中的 Database* g_database
- 删除 GetDatabase() 方法
- 删除 SetDatabase() 方法

# HashFamily
- 删除 static Database* g_db
- 删除 GetDatabase() 方法
- 删除 SetDatabase() 方法

# SetFamily 和 ListFamily 同样处理
```

#### 0.5.3 定义 CommandContext
```cpp
// include/core/command_context.h
#pragma once

class EngineShard;  // 前向声明

struct CommandContext {
    EngineShard* local_shard = nullptr;
    size_t shard_count = 1;
    size_t db_index = 0;
    void* connection = nullptr;

    // 便捷方法：获取当前 Database
    Database& GetDB() {
        return local_shard->GetDB();
    }
};
```

#### 0.5.4 修改所有命令签名
```bash
# 需要修改的命令签名
- StringFamily::Set(args) → Set(args, ctx)
- StringFamily::Get(args) → Get(args, ctx)
- HashFamily::HSet(args) → HSet(args, ctx)
- SetFamily::SAdd(args) → SAdd(args, ctx)
- ListFamily::LPush(args) → LPush(args, ctx)

# 或者：修改 CommandRegistry 直接传递 CommandContext
```

#### 0.5.5 单元测试
```bash
# 验证修复后的线程安全性
- 测试多线程并发读写
- 使用 TSAN 检测数据竞争
- 使用 Valgrind 检测内存错误
```

**测试：**
- [ ] 所有现有命令仍然工作（通过 CommandContext）
- [ ] 单元测试通过
- [ ] TSAN 无数据竞争报警
- [ ] redis-benchmark 验证性能（单线程模式）

---

### 阶段 1：引入 CommandContext（2-3 天）

**目标：** 为命令执行添加上下文，为后续分片做准备

**任务：**

#### 1.1 定义 CommandContext
```cpp
// include/core/command_context.h
#pragma once

class EngineShard;  // 前向声明

struct CommandContext {
    EngineShard* local_shard = nullptr;
    size_t shard_count = 1;
    size_t db_index = 0;
    void* connection = nullptr;

    // 辅助函数
    bool IsSingleShard() const { return shard_count == 1; }
};
```

#### 1.2 修改 CommandRegistry
```cpp
// include/command/command_registry.h
class CommandRegistry {
public:
    // 旧接口（保持兼容）
    using CommandHandler = std::function<std::string(const std::vector<CompactObj>&)>;

    // 新接口（支持上下文）
    using CommandHandlerWithContext = std::function<std::string(const std::vector<CompactObj>&, CommandContext*)>;

    void register_command(const std::string& name, CommandHandler handler);
    void register_command_with_context(const std::string& name, CommandHandlerWithContext handler);
    std::string execute(const std::vector<CompactObj>& args, CommandContext* ctx = nullptr);
};
```

#### 1.3 修改 Server 处理流程
```cpp
// src/server/server.cc
std::string RedisServer::process_command(const std::vector<CompactObj>& args) {
    CommandContext ctx;
    ctx.db_index = store_.CurrentDB();
    ctx.shard_count = 1;

    // 优先使用带上下文的处理器
    return CommandRegistry::instance().execute(args, &ctx);
}
```

**测试：**
- [ ] 所有现有命令仍然工作
- [ ] 单元测试通过
- [ ] redis-benchmark 验证

---

### 阶段 2：实现核心组件（3-4 天）

**目标：** 实现 TaskQueue 和 EngineShard 基础设施

**先决条件：**
- [ ] 阶段 0.5 已完成：Database 全局指针问题已修复

**任务：**

#### 2.1 实现 TaskQueue
```cpp
// include/core/task_queue.h
#pragma once
#include <atomic>
#include <functional>
#include <photon/thread/thread.h>
#include <photon/common/alog.h>
#include <sys/eventfd.h>

class TaskQueue {
public:
    explicit TaskQueue(size_t capacity = 4096);
    ~TaskQueue();

    template<typename F>
    bool TryAdd(F&& func);

    template<typename F>
    bool Add(F&& func);

    template<typename F>
    auto Await(F&& func) -> decltype(func());

    void ProcessTasks();
    bool Empty() const { return head_.load() == tail_.load(); }

private:
    static constexpr size_t kCapacity = 4096;
    static constexpr size_t kMask = kCapacity - 1;

    struct Task {
        std::function<void()> func;
        void* result_storage = nullptr;
        photon::semaphore* completion_signal = nullptr;
    };

    std::atomic<uint64_t> head_{0};
    std::atomic<uint64_t> tail_{0};
    std::unique_ptr<Task[]> buffer_;
    int event_fd_;
};
```

**关键实现细节：**
- 使用 `photon::semaphore` 进行 Await 同步
- 使用 `std::aligned_storage` 存储结果
- 使用 `event_fd` 通知任务到达

#### 2.2 实现 EngineShard
```cpp
// include/server/engine_shard.h
#pragma once
#include <array>
#include <memory>
#include <thread>
#include <atomic>
#include <string>

#include "core/dashtable.h"
#include "core/compact_obj.h"
#include "core/task_queue.h"
#include "photon/net/socket.h"
#include "photon/thread/thread.h"
#include "photon/common/alog.h"

class EngineShardSet;  // 前向声明

class EngineShard {
public:
    static const size_t kNumDBs = 16;
    using DbIndex = size_t;

    EngineShard(size_t shard_id, uint16_t port, EngineShardSet* shard_set);
    ~EngineShard();

    // Thread-local access
    static EngineShard* tlocal() { return tlocal_shard_; }
    static void set_tlocal(EngineShard* shard) { tlocal_shard_ = shard; }

    // Shard identification
    size_t shard_id() const { return shard_id_; }

    // Database access
    Database& GetDB() { return *db_; }

    // Task queue
    TaskQueue* GetTaskQueue() { return &task_queue_; }

    // Lifecycle
    void Start();
    void Stop();
    void Join();

private:
    // Main event loop
    void EventLoop();

    // Shard ID
    size_t shard_id_;

    // Data storage: 使用现有的 Database 类
    std::unique_ptr<Database> db_;

    // Cross-shard task queue (MPSC)
    TaskQueue task_queue_;

    // Worker thread (OS thread, not photon fiber)
    std::thread thread_;
    std::atomic<bool> running_;

    // Thread-local pointer
    static __thread EngineShard* tlocal_shard_;
};
```

**关键设计决策：**
- **复用现有的 Database 类**，避免重写数据层
- EngineShard 包含一个 Database 实例
- **暂时不实现 SO_REUSEPORT**，先用单线程验证
- EventLoop 简单地处理任务队列

**测试：**
- [ ] TaskQueue 单元测试（生产者-消费者模式）
- [ ] EngineShard 单元测试（独立线程）
- [ ] 跨线程任务分发测试

---

### 阶段 3：引入 ShardedServer（2-3 天）

**目标：** 创建支持多 shard 的服务器，但保持单监听器

**先决条件：**
- [ ] 阶段 0.5 已完成：Database 全局指针问题已修复
- [ ] 阶段 1 已完成：CommandContext 已引入
- [ ] 阶段 2 已完成：核心组件已实现

**任务：**

#### 3.1 实现 EngineShardSet
```cpp
// include/server/engine_shard_set.h
#pragma once
#include <vector>
#include <memory>
#include "server/engine_shard.h"

class EngineShardSet {
public:
    explicit EngineShardSet(size_t num_shards);

    template<typename F>
    auto Await(size_t shard_id, F&& func) -> decltype(func());

    template<typename F>
    void Add(size_t shard_id, F&& func);

    EngineShard* GetShard(size_t shard_id) { return shards_[shard_id].get(); }
    size_t size() const { return shards_.size(); }

    void Start();
    void Stop();
    void Join();

private:
    std::vector<std::unique_ptr<EngineShard>> shards_;
};
```

#### 3.2 实现 ShardedServer
```cpp
// include/server/sharded_server.h
#pragma once
#include <memory>
#include "server/engine_shard_set.h"
#include "server/server.h"

class ShardedServer {
public:
    ShardedServer(size_t num_shards, int port);
    ~ShardedServer();

    int Run();
    void Term();

private:
    // 保留原有的 RedisServer 作为单线程模式
    std::unique_ptr<RedisServer> single_threaded_server_;

    // 新的多线程模式
    std::unique_ptr<EngineShardSet> shard_set_;
    size_t num_shards_;
    int port_;

    // 命令处理（分发到正确的 shard）
    std::string DispatchCommand(const std::vector<CompactObj>& args);
};
```

**关键设计决策：**
- **保留原有的 RedisServer**，用于单线程模式
- 新的 ShardedServer 负责多 shard 协调
- **暂时使用单一监听器**，所有连接分发到 shard 0
- 后续阶段再实现 SO_REUSEPORT

**测试：**
- [ ] 启动 4 个 shard
- [ ] 连接到服务器
- [ ] 基本命令（GET/SET）工作
- [ ] 跨 shard 任务分发工作

---

### 阶段 4：实现 SO_REUSEPORT（3-4 天）

**目标：** 每个 shard 独立监听连接

**先决条件：**
- [ ] 阶段 2 已完成：EngineShard 基础设施工作正常

**任务：**

#### 4.1 修改 EngineShard::EventLoop
```cpp
void EngineShard::EventLoop() {
    set_tlocal(this);

    // 1. 创建 SO_REUSEPORT socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // 绑定并监听
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock_fd, 128);

    // 2. 将原生 fd 包装为 Photon 的 Socket 对象
    auto server_socket = photon::net::new_tcp_socket(sock_fd);

    // 3. 启动纤程 A：专门处理 Accept
    photon::thread_create11([this, server_socket]() {
        while (running_) {
            // 这里调用 photon 的 accept，它会挂起当前纤程，不会阻塞线程
            auto client_sock = server_socket->accept();
            if (client_sock) {
                // 为每个连接启动一个新纤程进行处理
                photon::thread_create11([this, client_sock]() {
                    HandleConnection(client_sock);
                    delete client_sock; // 处理完关闭
                });
            } else {
                photon::thread_yield(); // 出错稍微让出一下 CPU
            }
        }
    });

    // 4. 启动纤程 B：专门处理 TaskQueue
    photon::thread_create11([this]() {
        uint64_t buf;
        while (running_) {
            // 这里的 read 是 photon hook 过的，会挂起纤程等待 event_fd 可读
            // 只要没有任务，这个纤程就会睡眠，不消耗 CPU
            ssize_t ret = photon::net::read(task_queue_.event_fd(), &buf, sizeof(buf));
            if (ret > 0) {
                task_queue_.ProcessTasks();
            }
        }
    });

    // 5. 主纤程进入休眠
    // 因为 Accept 和 TaskQueue 都在独立的纤程里跑，这里只需要保活即可
    while (running_) {
        photon::thread_sleep(1); // 每秒醒来一次
        // 可以在这里做清理过期 Key 等工作
    }

    delete server_socket;
}
```

**测试：**
- [ ] 4 个 shard 同时监听
- [ ] 多个客户端连接
- [ ] 连接被正确分发到不同 shard
- [ ] 负载均衡合理

---

### 阶段 5：实现跨 Shard 命令（3-5 天）

**目标：** 实现跨 shard 的命令（MGET/MSET）

**任务：**

#### 5.1 添加 Shard 辅助函数
```cpp
// include/server/sharding.h
#pragma once
#include <string_view>
#include <xxhash.h>

inline size_t Shard(std::string_view key, size_t num_shards) {
    if (num_shards == 1) return 0;
    uint64_t hash = XXH64(key.data(), key.size(), 0);
    return hash % num_shards;
}
```

#### 5.2 修改命令实现

**⚠️ 重要原则：永远不要在 Photon 线程中使用 std::async 或 std::future！**

**正确示例：**
```cpp
// ✅ 使用 Photon 纤程
std::vector<photon::thread*> fibers;
for (const auto& [shard_id, keys] : keys_by_shard) {
    fibers.push_back(photon::thread_create11([shard_id, keys]() {
        // 执行任务...
    }));
}

// 等待所有纤程完成
for (auto* fiber : fibers) {
    photon::thread_join(fiber);  // ✅ 只挂起当前纤程
}
```

**详细实现：** 见 `doc/mget_mset_correct_implementation.md`

**测试：**
- [ ] SET/GET 在本地 shard 工作
- [ ] SET/GET 在远程 shard 工作
- [ ] MGET 并行获取多个 shard
- [ ] 内存泄漏检测（valgrind）

---

### 阶段 6：全局命令和优化（3-4 天）

**目标：** 实现全局命令（SELECT/FLUSHDB/DBSIZE）和性能优化

**任务：**

#### 6.1 实现全局命令
```cpp
// SELECT: 修改每个连接的 DB 索引
// FLUSHDB: 广播到所有 shard
// DBSIZE: 聚合所有 shard 的 key count
// KEYS: 聚合所有 shard 的 keys
```

#### 6.2 性能优化
- [ ] CPU 亲和性设置
- [ ] 本地 key 快速路径
- [ ] 批量任务处理
- [ ] 性能监控

#### 6.3 完整测试
- [ ] redis-benchmark 压测
- [ ] 24 小时稳定性测试
- [ ] 内存泄漏检测
- [ ] 竞争条件检测（TSAN）

---

## 回滚策略

每个阶段都有独立的 commit 点，可以随时回滚：

```bash
# 每个阶段结束后
git tag phase_0.5_completed  # 修复 Database 全局指针问题
git tag phase_1_completed  # 引入 CommandContext
git tag phase_2_completed  # 实现核心组件
git tag phase_3_completed  # 引入 ShardedServer
git tag phase_4_completed  # 实现 SO_REUSEPORT
git tag phase_5_completed  # 实现跨 Shard 命令
git tag phase_6_completed  # 全局命令和优化
```

如果某个阶段出现问题：
```bash
git checkout phase_X_completed  # 回滚到上一个稳定版本
```

---

## 时间估算

| 阶段 | 工作日 | 主要风险 |
|------|--------|----------|
| 阶段 0.5: 修复 Database 全局指针问题 | 2-3 | 🔴 高 - 致命性问题 |
| 阶段 1: 引入 CommandContext | 2-3 | 中 - 现有命令兼容性 |
| 阶段 2: 核心组件 | 3-4 | 中 - TaskQueue 实现 |
| 阶段 3: ShardedServer | 2-3 | 中 - 跨线程通信 |
| 阶段 4: SO_REUSEPORT | 3-4 | 中 - 连接分发 |
| 阶段 5: 跨 Shard 命令 | 3-5 | 高 - 对象生命周期 |
| 阶段 6: 全局命令和优化 | 3-4 | 中 - 性能调试 |
| **总计** | **18-25 工作日** | |

---

## 风险和缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 🔴 Database 全局指针 | 致命 | **阶段 0.5 优先修复**，使用 TSAN 验证 |
| 现有命令不兼容 | 高 | 保留旧接口，新旧并存 |
| TaskQueue 性能瓶颈 | 中 | 使用 lock-free 实现 |
| 连接分发不均 | 中 | 监控 metrics，考虑一致性哈希 |
| 内存泄漏 | 高 | 使用 valgrind 定期测试 |
| 跨 shard 原子性问题 | 中 | 文档清楚说明限制 |

---

## 成功标准

### 阶段性目标
- [ ] 每个阶段测试通过
- [ ] 没有明显的性能回退
- [ ] 代码质量符合项目标准

### 最终目标
- [ ] 支持 2-8 个 shard 配置
- [ ] 性能提升 3-5x（4 核机器）
- [ ] 所有 Redis 命令正常工作
- [ ] 通过 24 小时压力测试
- [ ] 没有内存泄漏
- [ ] 代码清晰、易维护

---

## 下一步行动

### 立即行动（今天）

1. **🔴 优先级最高：修复 Database 全局指针问题**
   - [ ] 审查所有 command family 的 Database 使用
   - [ ] 确认全局指针问题范围
   - [ ] 创建详细修复计划并评审
   - [ ] 先修复 Database 问题，再进行后续阶段

2. **运行原型测试**
   ```bash
   # 编译并运行原型测试
   cd /home/ubuntu/nano_redis
   cmake -B build -S .
   cmake --build build --target test_so_reuseport
   cmake --build build --target test_task_queue
   ./build/tests/prototype/test_so_reuseport
   ./build/tests/prototype/test_task_queue
   ```

### 本周内

- [ ] 完成阶段 0.5：修复 Database 全局指针问题
- [ ] 使用 TSAN 验证修复正确性
- [ ] 更新重构计划文档

### 下周

- [ ] 阶段 1：引入 CommandContext
- [ ] 阶段 2：实现核心组件
- [ ] 进行中期代码评审

---