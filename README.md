# Nano-Redis - A Modern C++ Redis Implementation

## 概述

Nano-Redis 是一个教学用的 Redis 实现，专注于展示现代 C++ 编程技巧、高性能数据结构和最新的操作系统接口。

## 项目目标

- 📚 **教学导向**：每个 git 提交都是一节完整的课程
- 🎯 **代码精简**：总代码量控制在 5000 行以内
- 🚀 **技术前沿**：使用 io_uring、C++20、Swiss Table 等
- 🔧 **实战导向**：每个组件都经过测试和性能基准

## 技术栈

| 组件 | 技术选择 | 原因 |
|------|---------|------|
| 网络 I/O | io_uring | 零拷贝、批量 syscall、高性能 |
| 异步模型 | C++20 Coroutines | 现代化异步编程 |
| 主存储 | Abseil flat_hash_map | Swiss Table，SIMD 加速 |
| 列表 | Abseil InlinedVector | 缓存友好，小列表零分配 |
| 字符串 | string_view + Cord | 零拷贝、COW |
| 过期管理 | Time Wheel | O(1) 定时器操作 |
| 内存分配 | Arena Allocator | 批量分配、无碎片 |

## 开发进度

**当前进度**: 1/18 提交完成 (5.6%)

| 提交 | 功能 | 状态 | 代码量 |
|------|------|------|--------|
| 1 | 项目脚手架 | ✅ | ~180 行 |
| 2 | Arena Allocator | ⏳ | ~300 行 |
| 3 | flat_hash_map vs unordered_map | ⏳ |
| 4 | string_view vs string vs Cord | ⏳ |
| 5 | 单元测试框架 | ⏳ |
| 6 | Socket 基础 | ⏳ |
| 7 | RESP 协议解析 | ⏳ |
| 8 | 命令注册和路由 | ⏳ |
| 9 | String 类型基础操作 | ⏳ |
| 10 | Hash 类型 | ⏳ |
| 11 | List 类型 | ⏳ |
| 12 | Set 类型 | ⏳ |
| 13 | 过期管理 | ⏳ |
| 14 | 完整命令集 | ⏳ |
| 15 | 性能分析 | ⏳ |
| 16 | io_uring 迁移 (Read/Write) | ⏳ |
| 17 | io_uring 迁移 (Accept/Close/批量) | ⏳ |
| 18 | 总结和展望 | ⏳ |

## 构建和运行

```bash
# 使用 CMake 构建
mkdir build && cd build
cmake ..
cmake --build .

# 运行测试
./tests/version_test

# 运行基准测试（后续阶段）
./tests/arena_bench
```

## 依赖

- **Abseil-Cpp**: 选择性使用特定组件
- **GoogleTest**: 单元测试框架
- **Linux 5.1+**: io_uring 支持

## 学习资源

### 完整方案文档

- **`PROJECT_PLAN.md`** ⭐ - 完整的 18 个提交详细方案（1262 行）
  - 每个提交的设计决策
  - 技术栈和性能对比
  - 学习目标和验收标准

- **`QUICK_REFERENCE.md`** - 快速参考指南
  - 18 个提交一览表
  - 代码量统计和进度
  - 快速命令和文件导航

### 每个提交文档

每个提交都包含完整的教学文档：
- `docs/DESIGN.md`: 设计决策说明
- `docs/ARCHITECTURE.md`: 架构图和设计原理
- `docs/PERFORMANCE.md`: 性能分析和优化
- `docs/LESSONS_LEARNED.md`: 学习要点总结

## 许可证

Apache License 2.0

## 贡献

本项目为教学目的，欢迎提出问题和建议。
