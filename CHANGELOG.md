# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Commit 06**: Socket 基础 - PhotonLibOS Echo 服务器
  - EchoServer 类实现 (`include/nano_redis/echo_server.h`, `src/echo_server.cc`)
  - PhotonLibOS 集成（协程 + epoll 网络层）
  - Pimpl 设计模式隐藏 PhotonLibOS 依赖
  - 协程风格的网络编程（类似 Go goroutine）
  - 所有测试通过

- **Commit 05**: 测试框架基础设施
  - 测试工具类 (`test_utils.h`): Timer, Benchmark, RandomStringGenerator, Status 断言宏
  - 统一的测试入口 (`test_main.cc`)
  - 测试工具使用示例 (`test_utils_example_test.cc`)
  - 测试框架设计文档 (`docs/TESTING_DESIGN.md`)
  - 测试学习要点文档 (`docs/TESTING_LESSONS.md`)

## [0.1.0] - 2024-12-24

### Added
- **Commit 01**: 项目脚手架
  - 版本管理 (`version.h`, `version.cc`)
  - 错误处理 (`status.h`)
  - 基础测试框架 (GoogleTest)
  - CMake 构建系统
  - Abseil 集成

- **Commit 02**: Arena 内存分配器
  - Arena 类实现
  - 指针 bump 分配策略
  - 自动内存块管理
  - 性能基准测试

- **Commit 03**: String Store 数据结构
  - 使用 `absl::flat_hash_map` 实现字符串存储
  - Get/Set/Del/Exists 基础操作
  - 性能基准测试对比 `std::unordered_map`

- **Commit 04**: String Utils 工具
  - 字符串工具函数
  - SSO (Small String Optimization) 示例
  - 字符串性能测试

[Unreleased]: https://github.com/yourusername/nano-redis/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/yourusername/nano-redis/releases/tag/v0.1.0
