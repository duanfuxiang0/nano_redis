# 使用 Valgrind 排查随机 Coredump 问题

## 问题描述

在运行单元测试时，发现测试有时正常通过，有时会发生 `Segmentation fault (core dumped)`：

```
[ RUN      ] StringFamilyTest.IncrNewKey
Segmentation fault (core dumped)
```

这种**非确定性的crash**通常意味着：
- 未初始化的内存
- 竞态条件（Race Condition）
- 悬垂指针

## 初步排查

### 1. 单独运行失败测试

```bash
./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey
```

**结果**：测试通过，无法稳定复现

### 2. 多次运行尝试复现

```bash
for i in {1..10}; do echo "Run $i:"; ./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey 2>&1 | tail -3; done
```

**结果**：10次全部通过，确认是**非确定性问题**

## 使用 Valgrind

### Valgrind 简介

Valgrind 是一套内存调试工具，主要功能：
- **Memcheck**：检测内存错误（未初始化内存、内存泄漏等）
- **Callgrind**：性能分析
- **Helgrind**：线程竞争检测

### 安装 Valgrind

```bash
sudo apt-get install valgrind
```

### 运行 Valgrind

```bash
valgrind --tool=memcheck --leak-check=full ./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey
```

**参数说明**：
- `--tool=memcheck`：使用内存检测工具
- `--leak-check=full`：详细的内存泄漏报告

## 分析 Valgrind 输出

### 关键错误信息

```
==1342773== Conditional jump or move depends on uninitialised value(s)
==1342773==    at 0x1AEB19: CompactObj::clear() (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x1AEBEB: CompactObj::setInt(long) (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x1AE148: CompactObj::CompactObj(long) (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x1AE4E6: CompactObj::fromInt(long) (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x17A1CE: StringFamily::Incr[abi:cxx11](std::vector<CompactObj, std::allocator<CompactObj> > const&)
```

### 问题分析

1. **错误类型**：`Conditional jump or move depends on uninitialised value(s)`
   - 表示条件判断使用了未初始化的变量

2. **调用栈分析**：
   ```
   CompactObj::CompactObj(long)        // 构造函数
   └─> CompactObj::setInt(long)       // 设置整数值
       └─> CompactObj::clear()        // 清理旧数据
           └─> getTag()               // 读取 taglen_ [问题点！]
   ```

3. **根本原因**：
   - 构造函数直接调用 `setInt(val)`
   - `setInt()` 调用 `clear()` 释放旧资源
   - `clear()` 读取 `taglen_` 判断是否需要释放
   - 但此时 `taglen_` **从未被初始化**，包含垃圾值

## 定位问题代码

### 问题代码 (compact_obj.cc:40-42)

```cpp
CompactObj::CompactObj(int64_t val) {
    setInt(val);  // ❌ 直接调用，taglen_/flag_ 未初始化
}
```

### 为什么有时正确？

这是**未定义行为 (Undefined Behavior)** 的典型特征：
- 未初始化的变量包含栈上的随机垃圾值
- 有时垃圾值恰好让 `clear()` 正确执行
- 有时垃圾值导致错误的内存访问，触发 segfault

## 修复方案

### 修复后的代码

```cpp
CompactObj::CompactObj(int64_t val) {
    setTag(NULL_TAG);  // ✅ 先初始化成员变量
    setFlag(0);
    setInt(val);
}
```

同样修复 `string_view` 构造函数：

```cpp
CompactObj::CompactObj(std::string_view str) {
    setTag(NULL_TAG);  // ✅ 先初始化成员变量
    setFlag(0);
    setString(str);
}
```

### 修复原理

1. **初始化顺序**：在使用成员变量前先初始化
2. **设置安全初始值**：`NULL_TAG` 和 `0` 确保 `clear()` 正确执行
3. **避免未定义行为**：消除所有读取未初始化变量的可能性

## 验证修复

### 1. Valgrind 重新检测

```bash
valgrind --tool=memcheck --leak-check=full ./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey
```

**结果**：
```
==1344616== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### 2. 内存泄漏检测

```
==1344616== All heap blocks were freed -- no leaks are possible
```

### 3. 稳定性测试

```bash
for i in {1..20}; do echo "Run $i:"; ./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey 2>&1 | tail -3; done
```

**结果**：20次全部通过，不再出现 coredump

### 4. 完整测试套件

```bash
./build/unit_tests
```

**结果**：所有 109 个测试通过

```bash
valgrind --tool=memcheck --leak-check=full ./build/unit_tests
```

**结果**：
```
[==========] 109 tests from 6 test suites ran. (5 ms total)
[  PASSED  ] 109 tests.
==1345227== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
==1345227== All heap blocks were freed -- no leaks are possible
```

## Valgrind 常用命令

### 基本用法

```bash
# 内存检测
valgrind --tool=memcheck ./your_program

# 详细泄漏检测
valgrind --tool=memcheck --leak-check=full ./your_program

# 显示泄漏详情
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./your_program

# 追踪未初始化值的来源
valgrind --tool=memcheck --track-origins=yes ./your_program
```

### 高级选项

```bash
# 生成抑制文件（忽略已知问题）
valgrind --tool=memcheck --gen-suppressions=all ./your_program > suppressions.supp

# 使用抑制文件
valgrind --tool=memcheck --suppressions=suppressions.supp ./your_program

# 只显示错误信息
valgrind --tool=memcheck -q ./your_program

# 输出到文件
valgrind --tool=memcheck --log-file=valgrind.log ./your_program
```

### 竞态条件检测

```bash
valgrind --tool=helgrind ./your_program
```

## Valgrind 错误类型速查

| 错误类型 | 含义 | 严重程度 |
|---------|------|---------|
| `Invalid read` | 读取了无效内存地址 | 🔴 高 |
| `Invalid write` | 写入了无效内存地址 | 🔴 高 |
| `Conditional jump or move depends on uninitialised value(s)` | 使用未初始化变量做判断 | 🟠 中 |
| `Use of uninitialised value` | 使用未初始化变量的值 | 🟠 中 |
| `Leak still reachable` | 内存泄漏（指针仍可达） | 🟡 低 |
| `Leak definitely lost` | 内存泄漏（指针丢失） | 🟡 低 |
| `Mismatched free()` | 错误的释放方式 | 🟠 中 |

## 经验教训

### 1. 非确定性 crash 的排查流程

1. 尝试稳定复现
2. 使用 Valgrind 等工具检测
3. 分析调用栈定位问题代码
4. 修复并验证

### 2. 构造函数的最佳实践

```cpp
// ✅ 推荐：先初始化所有成员
MyClass::MyClass(int val) {
    member1_ = nullptr;      // 先初始化
    member2_ = 0;
    setValue(val);           // 再使用
}

// ❌ 避免：直接使用未初始化成员
MyClass::MyClass(int val) {
    setValue(val);  // setValue() 可能读取未初始化的 member1_
}
```

### 3. 类成员初始化的最佳实践

```cpp
class MyClass {
public:
    // ✅ 推荐1：使用成员初始化列表
    MyClass(int val) : member1_(nullptr), member2_(0) {
        setValue(val);
    }

    // ✅ 推荐2：默认成员初始化器 (C++11)
    MyClass(int val) {
        // member1_ 和 member2_ 在类定义中初始化
        setValue(val);
    }

private:
    std::string* member1_ = nullptr;  // C++11 默认初始化
    int member2_ = 0;
};
```

## 总结

本次问题排查展示了 Valgrind 在定位非确定性内存问题中的强大能力：

1. **快速定位**：一次 Valgrind 运行就找到了未初始化变量问题
2. **详细调用栈**：清晰显示问题发生的完整调用链
3. **零开销检测**：不需要修改代码就能检测
4. **全面验证**：修复后可验证所有内存问题都已解决

对于任何非确定性的 crash，**Valgrind 应该是首选工具**。
