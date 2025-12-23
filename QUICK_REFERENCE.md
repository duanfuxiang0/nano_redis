# Nano-Redis 教程快速参考

## 📁 当前项目状态

| 项目指标 | 状态 |
|---------|------|
| **Git 提交** | 2 个 (Commit 01 + 计划文档) |
| **完成阶段** | 1/18 提交 (5.6%) |
| **有效代码** | ~200 行 |
| **文档** | ~2000 行 |

---

## 📝 18 个提交快速概览

### 🟢 第一阶段：基础设施（5 个提交）

| # | 提交名称 | 代码量 | 关键技术 |
|---|---------|--------|---------|
| 1 | ✅ Hello World - 项目脚手架 | ~180 | constexpr, enum class, inline, GoogleTest |
| 2 | Arena Allocator | ~300 | 指针 bump 分配, 批量释放 |
| 3 | flat_hash_map vs unordered_map | ~350 | Swiss Table, SIMD 探测, open addressing |
| 4 | string_view vs string vs Cord | ~200 | 零拷贝, COW, 异构查找 |
| 5 | 单元测试框架 | ~250 | TDD, 基准测试 |

### 🟡 第二阶段：网络层和协议（4 个提交）

| # | 提交名称 | 代码量 | 关键技术 |
|---|---------|--------|---------|
| 6 | Socket 基础 - 同步 Echo | ~300 | TCP socket, epoll, 事件驱动 |
| 7 | RESP 协议解析 | ~350 | 流式解析, 零拷贝 string_view |
| 8 | 命令注册和路由 | ~280 | 命令模式, flat_hash_map 路由 |
| 9 | String 类型基础操作 | ~320 | GET/SET/DEL/EXISTS, flat_hash_map 存储 |

### 🔵 第三阶段：数据类型扩展（5 个提交）

| # | 提交名称 | 代码量 | 关键技术 |
|---|---------|--------|---------|
| 10 | Hash 类型 | ~280 | 嵌套 flat_hash_map, HGETALL |
| 11 | List 类型 | ~320 | InlinedVector, 双端操作, LRANGE |
| 12 | Set 类型 | ~260 | flat_hash_set, 集合运算 |
| 13 | 过期管理 | ~340 | Time Wheel, O(1) 定时器 |
| 14 | 完整命令集 | ~250 | 兼容性测试, redis-benchmark |

### 🔴 第四阶段：性能优化和高级特性（4 个提交）

| # | 提交名称 | 代码量 | 关键技术 |
|---|---------|--------|---------|
| 15 | 性能分析 | ~150 | perf 火焰图, 瓶颈识别 |
| 16 | io_uring 迁移 (Read/Write) | ~400 | io_uring SQE/CQE, 零拷贝 I/O |
| 17 | io_uring 迁移 (Accept/批量) | ~350 | 链接操作, 批量提交 |
| 18 | 总结和展望 | ~100 | 完整的 nano-redis, 性能基准 |

---

## 📊 代码量统计

```
阶段              | 提交 | 新增代码 | 累计代码
------------------|------|----------|----------
✅ 第一阶段：基础设施 | 1/5  | ~180    | ~180
⏳ 第二阶段：网络层   | 0/4  | ~1250   | ~1430
⏳ 第三阶段：数据类型 | 0/5  | ~1450   | ~2880
⏳ 第四阶段：性能优化 | 0/4  | ~1000   | ~3880
------------------|------|----------|----------
总计              | 1/18 | ~3880   | ~3880
目标              | 18/18 | ~4980   | ~4980
进度              | 5.6%  | 78%     | 78%
```

---

## 🎯 下一步：Commit 02 - Arena Allocator

### 预览

```cpp
// Arena Allocator 核心结构
class Arena {
  void* ptr_;                    // 当前分配位置
  void* end_;                    // 当前块结束位置
  size_t block_size_;             // 每块大小
  std::vector<void*> blocks_;     // 已分配的块

public:
  void* Allocate(size_t size, size_t alignment = 8);
  void Reset();
};
```

### 性能对比预期

```
操作              | Arena | malloc | 提升
------------------|-------|--------|------
分配 1000 小对象   | 0.01ms | 0.10ms | 10x
释放（批量）       | 0.001ms | N/A    | ∞
内存碎片          | 0%     | ~15%   | -
```

---

## 📚 文件导航

| 文件 | 描述 | 链接 |
|------|------|------|
| `PROJECT_PLAN.md` | **完整 18 提交详细方案** | [查看](./PROJECT_PLAN.md) |
| `README.md` | 项目概述 | [查看](./README.md) |
| `COMMIT_01_SUMMARY.md` | Commit 01 总结 | [查看](./COMMIT_01_SUMMARY.md) |
| `docs/DESIGN.md` | 设计决策（旧版本） | [查看](./docs/DESIGN.md) |

---

## 🚀 快速命令

```bash
# 查看项目结构
tree -L 2 -I 'build|.git'

# 查看完整计划
cat PROJECT_PLAN.md

# 查看最近的提交
git log --oneline -5

# 查看当前状态
git status

# 开始下一个提交
cd /home/ubuntu/nano_redis
# 准备实施 Commit 02...
```

---

## 🎓 学习路径建议

### 如果是 C++ 初学者
1. ✅ 已完成 Commit 01 - 基础语法
2. 📖 阅读: `docs/LESSONS_LEARNED.md`
3. 📝 练习: 修改版本号，运行测试
4. 🚀 下一步: Commit 02 - 内存分配基础

### 如果有经验的开发者
1. ✅ 已完成 Commit 01 - 项目脚手架
2. 📖 阅读: `PROJECT_PLAN.md` - 选择感兴趣的提交
3. 🧪 尝试: 自行实现某个模块
4. 🚀 建议: 可以跳过某些基础提交

### 如果想学习性能优化
1. ⏭ 快速浏览: Commit 01-09（了解架构）
2. 📖 深入学习: Commit 10-14（数据结构）
3. 🎯 重点实践: Commit 15-18（性能优化）
4. 📊 分析: 使用 perf 和基准测试

---

## ❓ 常见问题

### Q: 需要按顺序学习吗？
**A**: 建议按顺序，因为后续依赖前面的基础。但有经验的开发者可以跳过。

### Q: 代码量限制为什么是 5000 行？
**A**: 聚焦核心功能，便于教学和学习。完整 Redis 约 10 万行代码。

### Q: 可以只学习某个提交吗？
**A**: 可以！每个提交都是独立的课程，有完整的文档和测试。

### Q: 如何测试当前代码？
**A**:
```bash
cd /home/ubuntu/nano_redis
# 使用 CMake
mkdir build && cd build
cmake ..
cmake --build .
./tests/version_test

# 使用 Bazel（需要配置）
bazel build //tests/version_test
bazel test //tests/version_test
```

---

## 🎉 当前进度

```
进度: █████░░░░░░░░░░░░░░░░░░░ 5.6% (1/18)

📍 当前位置: Commit 01 ✅
🎯 下一个: Commit 02 - Arena Allocator
📅 预计完成时间: 每周 2-3 个提交，约 6-8 周
```

---

**💡 提示**: 查看完整方案，请打开 `PROJECT_PLAN.md` 文件！
