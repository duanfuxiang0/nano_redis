# Commit 1: Hello World - 项目脚手架

## 概述

本提交建立了 Nano-Redis 项目的完整脚手架，包括：
- 项目结构
- 构建系统（Bazel + CMake）
- 基础头文件（version.h, status.h）
- 单元测试框架

## 代码统计

```
文件                      | 行数 | 说明
--------------------------|------|------
include/nano_redis/version.h |   30 | 版本号定义
include/nano_redis/status.h  |   75 | 错误处理
src/version.cc              |   12 | 版本实现
tests/version_test.cc        |   60 | 单元测试
--------------------------|------|------
总计                      |  177 |
```

## 设计决策

### 1. 为什么选择双构建系统（Bazel + CMake）？

| 构建系统 | 优点 | 缺点 | 使用场景 |
|---------|------|--------|---------|
| **Bazel** | - 增量编译快<br/>- 依赖管理简单<br/>- 跨语言支持 | - 学习曲线陡峭<br/>- 安装复杂 | 开发阶段 |
| **CMake** | - 广泛支持<br/>- IDE 友好<br/>- 部署方便 | - 配置复杂<br/>- 速度较慢 | 生产环境 |

**决策**：同时支持两者，让用户根据场景选择。

### 2. 为什么分离头文件和实现？

```
include/nano_redis/     # 公共 API（稳定接口）
src/                  # 实现细节（内部可见）
tests/                # 测试代码
```

**优点**：
- 清晰的接口边界
- 便于库的使用者
- 减少编译依赖

### 3. Status 类的设计考虑

```cpp
enum class StatusCode : int {
  kOk = 0,
  kNotFound = 1,
  kInvalidArgument = 2,
  kInternalError = 3,
  kAlreadyExists = 4,
};
```

**为什么用 enum class？**
- 类型安全（避免隐式转换）
- 明确的作用域（kOk 而不是 OK）

**为什么不用 std::optional？**
- 需要传递错误信息（不仅仅是成功/失败）
- 后续扩展需要错误详情（message）
- 与 Abseil 的风格保持一致

## 架构图

```
nano_redis/
├── include/nano_redis/    ← 公共头文件
│   ├── version.h
│   └── status.h
├── src/                   ← 实现文件
│   └── version.cc
├── tests/                 ← 单元测试
│   └── version_test.cc
├── docs/                  ← 教学文档
│   ├── DESIGN.md
│   ├── ARCHITECTURE.md
│   ├── PERFORMANCE.md
│   └── LESSONS_LEARNED.md
├── BUILD.bazel             ← Bazel 构建
├── CMakeLists.txt          ← CMake 构建
└── README.md              ← 项目说明
```

## 性能分析

本阶段主要是脚手架代码，无性能关键路径。

### 编译时间

```bash
# CMake 增量编译
cmake --build build  # ~0.5s

# Bazel 增量编译
bazel build //...    # ~0.2s
```

## 学习要点

### C++20 特性

本提交使用的 C++20 特性：

1. **enum class**: 强类型枚举
   ```cpp
   enum class StatusCode : int { ... };
   ```

2. **constexpr**: 编译期常量
   ```cpp
   constexpr int kMajorVersion = 0;
   ```

3. **inline functions**: 头文件中定义函数
   ```cpp
   inline const char* GetVersion() { return kVersionString; }
   ```

### 项目组织

1. **Apache License 头注释**
   - 遵循开源协议规范
   - 便于第三方使用

2. **命名空间隔离**
   ```cpp
   namespace nano_redis {
     // 所有公共 API
   }
   ```

3. **包含保护宏**
   ```cpp
   #ifndef NANO_REDIS_VERSION_H_
   #define NANO_REDIS_VERSION_H_
   // ...
   #endif
   ```

### 测试框架

1. **GoogleTest 断言**
   - `EXPECT_*`: 失败继续执行
   - `ASSERT_*`: 失败立即停止

2. **测试命名规范**
   ```
   <Feature>Test, <TestName>
   例如: VersionTest, GetVersionReturnsCorrectString
   ```

## 下一步

下一个提交将实现 **Arena Allocator**，这是高性能内存管理的基础。

## 验收标准

- [x] 项目结构建立完成
- [x] Bazel 构建系统配置完成
- [x] CMake 构建系统配置完成
- [x] 版本号模块实现
- [x] 错误处理模块实现
- [x] 单元测试通过
- [x] 文档完整
