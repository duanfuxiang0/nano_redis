# Nano-Redis - CMU 15-445 风格 Mini-DragonflyDB

## 概述

Nano-Redis 是一个教学项目，通过 5 个 Lab 循序渐进实现 Mini-DragonflyDB，学习内存数据库的核心原理。

### 设计理念

- **CMU 15-445/645 课程风格**: 通过 Lab 循序学习数据库系统
- **参考 DragonflyDB**: 学习现代 Redis 实现的核心架构
- **代码量控制**: 保持在 5000 行内，聚焦核心原理
- **功能导向**: 代码按功能模块组织，便于维护

### 学习路径

```
Lab 0 (Socket + RESP) ← **已完成** ✅
    ↓
Lab 1 (HashMap + TTL)
    ↓
Lab 2 (高级数据结构)
    ↓
Lab 3 (分片架构) ← 分水岭：Mini-Redis → Mini-Dragonfly
    ↓
Lab 4 (持久化)
```

## 快速开始

### 构建项目

```bash
# 克隆仓库
git clone <repo-url>
cd nano_redis

# 构建
mkdir build && cd build
cmake ..
cmake --build .
```

### 运行服务器

```bash
# 启动服务器（默认 9527 端口）
./build/nano_redis_server --port=9527

# 使用 redis-cli 测试
redis-cli -p 9527 PING
redis-cli -p 9527 SET key value
redis-cli -p 9527 GET key

# 简单压测
redis-benchmark -p 9527 -t set,get,incr,lpush,lpop,sadd,spop,hset -q -n 100000 -c 100 -r 1000
```

### 运行测试

```bash
# 单元测试
cd build && ctest

# 性能基准
./build/benchmarks/hash_bench
./build/benchmarks/concurrent_bench
```

## 学习目标

通过本项目学习：

| Lab | 主题 | 学习重点 |
|-----|------|---------|
| 0 | 协议解析 | Socket 编程、RESP 协议、异步 I/O |
| 1 | 内存存储 | HashMap 设计、Rehash、TTL 过期 |
| 2 | 高级数据结构 | List、Hash、Set、ZSet 实现 |
| 3 | 并发架构 | 数据分片、Actor 模型、无锁设计 |
| 4 | 持久化 | AOF 日志、Snapshot 快照、Direct I/O |

## 技术栈

### 核心依赖

| 库 | 用途 | 对应 Dragonfly |
|-----|------|----------------|
| **Photon** | 异步 I/O、协程 | helio |
| **Abseil** | flat_hash_map、字符串 | Abseil |
| **gflags** | 命令行参数 | gflags |

### 数据结构选择

| 数据类型 | 实现 | 理由 |
|---------|------|------|
| 主存储 | `absl::flat_hash_map` | Swiss Table，成熟稳定 |
| List | `std::deque` | 双端队列，标准库 |
| Set | `absl::flat_hash_set` | Swiss Table 集合 |
| ZSet | `absl::btree_map` | 有序映射，简单高效 |

### 内存管理

- **默认**: mimalloc（本仓库自带 `third_party/mimalloc`，构建时静态链接并覆盖 `malloc/free/posix_memalign`）
- **Photon 栈分配**: 默认会为 fiber/线程栈走 `posix_memalign + mprotect + madvise`；本项目默认启用 Photon 的 `use_pooled_stack_allocator` 来减少频繁栈分配/回收的系统调用开销
- **可选**: 可用 `-DNANO_REDIS_USE_MIMALLOC=OFF` 回退到系统分配器（用于对比验证）
- **设计**: 无需自定义 Arena，学习重点在架构而非内存优化

## 项目结构

```
nano_redis/
├── include/                # 头文件
│   ├── base/             # 基础工具（Socket, Fiber）
│   ├── protocol/         # 协议层（RESP 解析/构建）
│   ├── core/          # 存储层（HashMap, 数据结构）
│   ├── server/           # 服务器层（分片、调度）
│   ├── persistence/      # 持久化层（AOF, Snapshot）
│   ├── command/         # 命令层（Family）
│   └── facade/          # 响应构建
├── src/                 # 实现文件
│   ├── base/
│   ├── protocol/
│   ├── core/
│   ├── server/
│   ├── persistence/
│   ├── command/
│   └── facade/
├── tests/               # 测试
│   ├── unit/           # 单元测试
│   ├── integration/    # 集成测试
│   └── data/          # 测试数据
├── benchmarks/          # 性能基准
├── third_party/
│   └── photonlibos/   # Photon I/O 库
├── CMakeLists.txt
├── server_main.cpp
├── README.md
├── AGENTS.md
└── LAB_PLAN.md         # Lab 0-4 详细规划
```

## 模块说明

### Protocol (协议层)

- **文件**: `protocol/resp_parser.h/cpp`, `protocol/resp_builder.h/cpp`
- **职责**: RESP 协议解析和序列化
- **参考**: Dragonfly `facade/redis_parser.h`

### core (存储层)

- **文件**:
  - `core/hash_table.h` - 哈希表接口
  - `core/simple_hash.h/cpp` - 拉链法实现
  - `core/dense_hash.h/cpp` - Dragonfly DenseSet
  - `core/hopscotch_hash.h/cpp` - 跳跃哈希
  - `core/db_slice.h/cpp` - 存储分片
  - `core/time_wheel.h/cpp` - TTL 时间轮
- **职责**: 键值存储、TTL 过期
- **参考**: Dragonfly `core/dense_set.h`

### Server (服务器层)

- **文件**:
  - `server/redis_server.h/cpp` - 主服务器
  - `server/engine_shard.h/cpp` - 单分片引擎
  - `server/engine_shard_set.h/cpp` - 分片集合
  - `server/task_queue.h/cpp` - 任务队列
- **职责**: 数据分片、任务调度、连接管理
- **参考**: Dragonfly `server/engine_shard*.h`

### Persistence (持久化层)

- **文件**:
  - `persistence/aof_writer.h/cpp` - AOF 写入
  - `persistence/aof_loader.h/cpp` - AOF 加载
  - `persistence/aof_rewrite.h/cpp` - AOF 重写
  - `persistence/snapshot.h/cpp` - RDB 快照
- **职责**: 数据持久化、恢复
- **参考**: Dragonfly `server/rdb_*.cc`, `server/journal/`

### Command (命令层)

- **文件**:
  - `command/command_id.h` - 命令元数据
  - `command/command_registry.h/cpp` - 命令注册
  - `command/string_family.h/cpp` - String 命令
  - `command/hash_family.h/cpp` - Hash 命令
  - `command/list_family.h/cpp` - List 命令
  - `command/set_family.h/cpp` - Set 命令
  - `command/zset_family.h/cpp` - ZSet 命令
- **职责**: 命令注册、路由、执行
- **参考**: Dragonfly `server/*_family.cc`

## DragonflyDB 参考

### 模块映射

| Nano-Redis | DragonflyDB | 学习重点 |
|-----------|-------------|---------|
| `protocol/` | `facade/redis_parser.h` | Socket, RESP |
| `core/simple_hash.h` | - | 基本哈希表原理 |
| `core/dense_hash.h` | `core/dense_set.h` | 内存优化技巧 |
| `core/ds_*.h` | `server/*_family.cc` | 数据结构设计 |
| `server/engine_shard.h` | `server/engine_shard.h` | 分片架构 |
| `persistence/aof_*.h` | `server/journal/` | AOF 机制 |
| `persistence/snapshot.h` | `server/rdb_*.cc` | Snapshot 机制 |

### 关键学习点

1. **并发模型**: Photon + Fiber 协程 vs Dragonfly Helio
2. **哈希表设计**: 开放寻址 vs 拉链法 vs DenseSet
3. **分片架构**: Shared-nothing 无锁设计
4. **Actor 模型**: 跨分片消息传递
5. **TTL 过期**: 惰性删除 vs 时间轮
6. **AOF**: 追加日志 vs Rewrite 压缩

## 命令支持

### Lab 1 (基础)
- SET, GET, DEL, EXISTS
- MSET, MGET
- APPEND, STRLEN
- INCR, DECR, INCRBY, DECRBY
- SELECT

### Lab 2 (高级数据结构)
- LPUSH, RPUSH, LPOP, RPOP
- LRANGE, LLEN, LINDEX, LTRIM
- HSET, HGET, HGETALL, HKEYS, HVALS
- HEXISTS, HDEL, HLEN, HINCRBY
- SADD, SMEMBERS, SISMEMBER
- SREM, SCARD, SUNION, SINTER
- ZADD, ZREM, ZRANGE, ZSCORE

### Lab 3 (并发)
- 所有 Lab 1-2 命令（多线程支持）

### Lab 4 (持久化)
- BGSAVE (Snapshot)
- BGREWRITEAOF (AOF Rewrite)

## 测试与基准

### 单元测试

```bash
cd build
ctest
```

### 性能基准

```bash
# 哈希表对比
./benchmarks/hash_bench

# 数据结构基准
./benchmarks/ds_bench

# 并发基准
./benchmarks/concurrent_bench --threads 1,2,4,8
```

### 使用 Redis-benchmark

```bash
redis-benchmark -h 127.0.0.1 -p 9527 -t set,get -n 100000 -c 10
```

## 开发规范

详细开发规范参见 [AGENTS.md](AGENTS.md)

### 核心原则

1. **代码风格**: 遵循 Dragonfly 风格
2. **模块化**: 每个模块职责单一
3. **测试优先**: 每个功能都有单元测试
4. **性能导向**: 基准测试驱动优化
5. **文档完善**: 代码注释和 Lab 文档同步

## 学习资源

- **CMU 15-445**: Database Systems 课程
- **DragonflyDB**: https://github.com/dragonflydb/dragonfly
- **Redis Protocol**: https://redis.io/topics/protocol
- **Photon**: https://github.com/alibaba/PhotonLibOS

## License

Apache License 2.0

## Contributing

本项目是教学项目，欢迎提交 Issue 和 PR。

---

## 快速参考

### Lab 0: 环境准备与协议解析 ✅ 已完成
- `protocol/resp_parser.h/cpp` - RESP 解析和构建
- `server/server.h/cpp` - Photon 服务器
- `server/connection.h/cpp` - 连接处理
- `command/command_registry.h/cpp` - 命令注册

### Lab 1: 内存存储引擎
- `core/hash_table.h` - 接口
- `core/simple_hash.h/cpp` - 拉链法
- `core/dense_hash.h/cpp` - DenseSet
- `core/hopscotch_hash.h/cpp` - 跳跃哈希
- `core/db_slice.h/cpp` - 存储分片
- `command/string_family.h/cpp` - String 命令

### Lab 2: 高级数据结构
- `core/ds_list.h/cpp` - List
- `core/ds_hash.h/cpp` - Hash
- `core/ds_set.h/cpp` - Set
- `core/ds_zset.h/cpp` - ZSet
- `command/hash_family.h/cpp` - Hash 命令
- `command/list_family.h/cpp` - List 命令
- `command/set_family.h/cpp` - Set 命令
- `command/zset_family.h/cpp` - ZSet 命令

### Lab 3: 并发架构与分片
- `server/engine_shard.h/cpp` - 单分片
- `server/engine_shard_set.h/cpp` - 分片集合
- `server/task_queue.h/cpp` - 任务队列
- `server/redis_server.h/cpp` - 主服务器

### Lab 4: 持久化与恢复
- `persistence/aof_writer.h/cpp` - AOF 写入
- `persistence/aof_loader.h/cpp` - AOF 加载
- `persistence/aof_rewrite.h/cpp` - AOF 重写
- `persistence/snapshot.h/cpp` - RDB 快照
