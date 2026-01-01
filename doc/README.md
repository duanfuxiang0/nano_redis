# 工具使用文档

本目录记录了项目中使用各种调试和开发工具的实践经验。

## 文档列表

### [Valgrind 调试工作流](./valgrind_debugging_workflow.md)

**内容**：使用 Valgrind 排查随机 Coredump 问题的完整流程

**适用场景**：
- 非确定性的程序崩溃
- 内存相关错误（未初始化内存、内存泄漏等）
- 难以复现的 bug

**涉及技术**：
- Valgrind Memcheck 工具使用
- 未初始化变量检测
- 调用栈分析
- 内存泄漏检测

**关键收获**：
- 如何使用 Valgrind 快速定位未初始化内存问题
- 构造函数成员初始化的最佳实践
- 非确定性问题的排查方法论

## 贡献指南

当你在项目中使用了有价值的工具或调试技巧时，欢迎添加到这个目录：

1. 创建新的 `.md` 文件
2. 记录完整的问题-解决过程
3. 包含命令示例和代码片段
4. 添加经验教训和最佳实践
5. 在本索引文件中添加条目

## 相关资源

- [Valgrind 官方文档](https://valgrind.org/docs/manual/)
- [C++ 内存调试技巧](https://github.com/isocpp/CppCoreGuidelines)
- [项目 AGENTS.md](../AGENTS.md) - 项目开发规范
