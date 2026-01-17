# 使用 Valgrind 排查随机 Coredump 问题

## 多次运行尝试复现

```bash
for i in {1..10}; do echo "Run $i:"; ./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey 2>&1 | tail -3; done
```

**结果**：10次全部通过，确认是**非确定性问题**

## 使用 Valgrind

```bash
sudo apt-get install valgrind
```

```bash
valgrind --tool=memcheck --leak-check=full ./build/unit_tests --gtest_filter=StringFamilyTest.IncrNewKey
```

**参数说明**：
- `--tool=memcheck`：使用内存检测工具
- `--leak-check=full`：详细的内存泄漏报告

### 关键错误信息

```
==1342773== Conditional jump or move depends on uninitialised value(s)
==1342773==    at 0x1AEB19: NanoObj::clear() (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x1AEBEB: NanoObj::setInt(long) (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x1AE148: NanoObj::NanoObj(long) (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x1AE4E6: NanoObj::fromInt(long) (in /home/ubuntu/nano_redis/build/unit_tests)
==1342773==    by 0x17A1CE: StringFamily::Incr[abi:cxx11](std::vector<NanoObj, std::allocator<NanoObj> > const&)
```

1. **错误类型**：`Conditional jump or move depends on uninitialised value(s)`
   - 表示条件判断使用了未初始化的变量

2. **调用栈分析**：
   ```
   NanoObj::NanoObj(long)        // 构造函数
   └─> NanoObj::setInt(long)       // 设置整数值
       └─> NanoObj::clear()        // 清理旧数据
           └─> getTag()               // 读取 taglen_ [问题点！]
   ```

3. **根本原因**：
   - 构造函数直接调用 `setInt(val)`
   - `setInt()` 调用 `clear()` 释放旧资源
   - `clear()` 读取 `taglen_` 判断是否需要释放
   - 但此时 `taglen_` **从未被初始化**，包含垃圾值

## Valgrind 常用命令

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

```bash
# 生成抑制文件
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