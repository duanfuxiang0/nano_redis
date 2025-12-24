# 文档更新日志 - 2025-01

## 📋 更新概览

根据项目需求，对整个项目的技术栈和文档进行了全面调整，以简化项目并提高兼容性。

## 🔧 主要变更

### 1. C++ 标准调整
- **从 C++20 降至 C++17**
  - 原因：提高编译器兼容性，更广泛的平台支持
  - 影响：移除了 `std::coroutine` 等C++20特性

### 2. 构建系统简化
- **移除 Bazel，仅保留 CMake**
  - 原因：简化项目结构，降低学习成本
  - 影响：删除了所有 BUILD.bazel 相关内容

### 3. 协程实现
- **C++20 协程 → 自实现 Task<T>**
  - 原因：C++17 兼容性，可控的执行模型
  - 实现：基于 `std::experimental::coroutine_handle` 或自定义协程机制

### 4. 字符串处理
- **Abseil string_view/Cord → std::string**
  - 原因：简化依赖，聚焦核心功能
  - 优化：利用 std::string 的 SSO (Small String Optimization)

### 5. 时间处理
- **Abseil Time → std::chrono**
  - 原因：使用标准库，减少外部依赖

### 6. Abseil 集成
- **包管理器 → add_subdirectory**
  - 实现方式：使用本地 Abseil 目录 `/home/ubuntu/abseil-cpp`
  - CMake 配置：
    ```cmake
    set(ABSL_PROPAGATE_CXX_STD ON)
    add_subdirectory(/home/ubuntu/abseil-cpp ${CMAKE_BINARY_DIR}/abseil-cpp EXCLUDE_FROM_ALL)
    ```

## 📄 文件修改列表

### 1. PROJECT_PLAN.md
**修改内容**：
- 添加"技术栈调整说明"章节
- 更新项目概览表（C++17、CMake、自实现 Task<T>）
- 修改 Commit 1：删除 Bazel 相关内容
- 修改 Commit 2：Arena 设计（C++17 风格）
- 修改 Commit 3：flat_hash_map（保持不变）
- 修改 Commit 4：字符串处理（改为 std::string）
- 修改 Commit 5：重新定义为测试工具库
- 修改 Commit 8：命令注册（自实现 Task<T>）
- 修改 Commit 9-14：所有命令实现（使用 std::string 和 std::chrono）
- 修改 Commit 16-17：io_uring（保持不变）
- 更新技术栈总结图
- 更新依赖和FAQ部分

### 2. README.md
**修改内容**：
- 更新技术栈表（C++17、自实现 Task<T>、std::string）
- 更新构建和运行说明（删除 Bazel）
- 更新依赖列表（Abseil 使用本地目录、C++17）

### 3. AGENTS.md
**修改内容**：
- 代码风格：C++20 → C++17
- 构建命令：删除 Bazel

### 4. CMakeLists.txt
**修改内容**：
- `CMAKE_CXX_STANDARD`: 20 → 17
- 添加 Abseil 集成配置

### 5. docs/DESIGN.md
**修改内容**：
- 设计决策：双构建系统 → 仅 CMake
- 架构图：删除 BUILD.bazel
- 编译时间：删除 Bazel
- 验收标准：添加 Abseil 集成

### 6. docs/ARCHITECTURE.md
**修改内容**：
- 更新技术栈章节

### 7. NEW - CHANGELOG.md
**新增文件**：本文档

## ✅ 验证结果

### CMake 配置
```bash
cmake ..
```
- ✅ C++17 标准识别成功
- ✅ Abseil C++17 检测通过（ABSL_INTERNAL_AT_LEAST_CXX17: Success）
- ✅ C++20 检测失败（符合预期，因为我们使用 C++17）

### 构建测试
```bash
cmake --build .
```
- ✅ libnano_redis.a 编译成功
- ✅ version_test 编译成功
- ✅ 所有测试通过（7/7）

### 测试运行
```bash
./version_test
```
- ✅ VersionTest: 2/2 通过
- ✅ StatusTest: 5/5 通过
- ✅ 总计：7/7 通过

## 📊 代码统计

| 文件 | 修改前 | 修改后 | 变化 |
|------|--------|--------|------|
| PROJECT_PLAN.md | 1263 行 | ~1350 行 | +87 行 |
| README.md | 101 行 | ~95 行 | -6 行 |
| AGENTS.md | 19 行 | 19 行 | 0 行（仅更新） |
| CMakeLists.txt | 41 行 | ~45 行 | +4 行 |
| docs/DESIGN.md | 171 行 | ~160 行 | -11 行 |

## 🎯 后续步骤

### 立即行动
1. ✅ 文档更新完成
2. ✅ CMake 配置验证通过
3. ⏳ **下一步**：执行 Commit 2 - Arena Allocator

### Commit 2 准备工作
需要创建的文件：
- `include/nano_redis/arena.h` - Arena 定义
- `src/arena.cc` - Arena 实现
- `tests/arena_test.cc` - 单元测试
- `tests/arena_bench.cc` - 性能基准测试
- `docs/arena_design.md` - Arena 设计文档

技术要点：
- 指针 bump 分配
- 多块管理（std::vector<void*>）
- Reset 功能
- MemoryUsage 统计

## Commit 3: flat_hash_map vs unordered_map - 数据结构选型

### 📋 概述

实现了基于 Swiss Table 的 `flat_hash_map` 作为 Redis 键值存储的基础数据结构，并提供了与标准库 `unordered_map` 的性能对比。

### 📄 新增文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/nano_redis/string_store.h` | 88 | 字符串存储定义 |
| `src/string_store.cc` | 129 | 字符串存储实现 |
| `tests/hash_table_bench.cc` | 180 | 性能基准测试 |
| **总计** | **397** | - |

### 🔧 修改文件

| 文件 | 修改内容 |
|------|----------|
| `CMakeLists.txt` | 添加 `string_store.cc` 到源文件列表，链接 `absl::flat_hash_map` |
| `docs/DESIGN.md` | 添加 Commit 3 设计决策说明 |
| `docs/ARCHITECTURE.md` | 添加 Swiss Table 内存布局和查找流程 |
| `docs/PERFORMANCE.md` | 添加 flat_hash_map vs unordered_map 性能对比 |
| `docs/LESSONS_LEARNED.md` | 添加哈希表、SIMD 指令学习要点 |
| `docs/DESIGN_swiss_table.md` | 新增 Swiss Table 设计文档 |

### 🎯 实现要点

#### 1. StringStore 类（flat_hash_map）
```cpp
class StringStore {
  using StringMap = absl::flat_hash_map<std::string, std::string>;
  StringMap store_;

 public:
  bool Put(const std::string& key, const std::string& value);
  bool Put(std::string&& key, std::string&& value);
  bool Get(const std::string& key, std::string* value) const;
  std::string* GetMutable(const std::string& key);
  bool Delete(const std::string& key);
  // ...
};
```

#### 2. StdStringStore 类（unordered_map）
```cpp
class StdStringStore {
  using StringMap = std::unordered_map<std::string, std::string>;
  StringMap store_;
  // 与 StringStore 完全相同的接口
};
```

#### 3. 性能基准测试
```cpp
// 测试不同数据量和字符串大小
BM_Insert<StringStore>/1000/8/8
BM_Insert<StringStore>/10000/8/8
BM_Insert<StringStore>/100000/8/8
BM_Lookup<StringStore>/...
BM_Delete<StringStore>/...
```

### 📊 预期性能对比

| 操作 | flat_hash_map | unordered_map | 提升 |
|------|--------------|---------------|------|
| Insert 100K | 120ms | 280ms | 2.3x |
| Lookup 100K | 80ms | 150ms | 1.9x |
| Delete 100K | 150ms | 200ms | 1.3x |
| Memory Usage | 64MB | 96MB | 1.5x |

### ✅ 验证结果

#### 构建验证
```bash
cmake --build .
```
- ✅ `libnano_redis.a` 编译成功（包含 `string_store.cc`）
- ✅ 所有测试通过

#### CMake 配置
- ✅ Abseil `absl::flat_hash_map` 链接成功
- ✅ 所有依赖解析正确

### 📚 技术文档

#### Swiss Table 核心概念
1. **Split Hash**: hash → H1 (bucket 选择) + H2 (SIMD 匹配)
2. **Control Bytes**: 每个槽对应一个控制字节（kEmpty, kDeleted, kSentinel, H2）
3. **Group Probing**: 16 个槽为一组，使用 SIMD 一次探测
4. **SIMD 探测**:
   ```cpp
   _mm_cmpeq_epi8(group, h2)  // 一次比较 16 个字节
   _mm_movemask_epi8(result)  // 提取匹配掩码
   ```

### 🚀 后续步骤

### 立即行动
1. ✅ Commit 3 完成
2. ⏳ **下一步**：执行 Commit 4 - std::string 高效使用

### Commit 4 准备工作
需要学习的主题：
- SSO (Small String Optimization) 原理
- 字符串拷贝优化技巧
- 零拷贝字符串处理

## 📚 技术亮点

### 1. C++17 兼容性
- 支持更广泛的编译器（GCC 8+, Clang 7+）
- 更好的企业环境兼容性
- 避免了 C++20 的某些边缘情况

### 2. 自实现 Task<T>
优势：
- 完全可控的执行模型
- 避免 C++20 协程的复杂性
- 可以深度定制（如调度策略）
- 教学价值高（理解协程原理）

### 3. std::string SSO
优势：
- 小字符串零堆分配（< 16-24 字节）
- 标准库支持，无需额外依赖
- 现代编译器优化良好

### 4. 简化的构建系统
优势：
- 只需要 CMake（无需 Bazel）
- 学习曲线平缓
- 易于 CI/CD 集成

## 🔗 相关文档

- [PROJECT_PLAN.md](./PROJECT_PLAN.md) - 完整项目计划
- [README.md](./README.md) - 项目概览
- [AGENTS.md](./AGENTS.md) - 代理开发指南
- [docs/DESIGN.md](./docs/DESIGN.md) - 设计决策文档

---

**更新日期**：2025-01
**更新人**：opencode
**版本**：v1.0
