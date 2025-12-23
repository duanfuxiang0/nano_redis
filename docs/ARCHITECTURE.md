# Commit 1: 架构说明

## 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    Nano-Redis 0.1.0                       │
├─────────────────────────────────────────────────────────────────┤
│                                                           │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │        Public API Layer (include/nano_redis/)        │  │
│  │                                                          │
│  │  ┌──────────┐  ┌──────────┐                          │  │
│  │  │ version  │  │ status   │                          │  │
│  │  │   .h    │  │   .h     │                          │  │
│  │  └──────────┘  └──────────┘                          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                        ▲                                   │
│                        │                                   │
│  ┌─────────────────────┴───────────────────────────────┐  │
│  │        Implementation Layer (src/)                   │  │
│  │                                                          │
│  │  ┌──────────┐                                          │  │
│  │  │ version  │                                          │  │
│  │  │   .cc    │                                          │  │
│  │  └──────────┘                                          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                        ▲                                   │
│                        │                                   │
│  ┌─────────────────────┴───────────────────────────────┐  │
│  │        Test Layer (tests/)                        │  │
│  │                                                          │
│  │  ┌──────────────┐                                      │  │
│  │  │ version_test │                                      │  │
│  │  │     .cc      │                                      │  │
│  │  └──────────────┘                                      │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                           │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │        Build Systems                                  │  │
│  │                                                          │
│  │  ┌──────────────┐  ┌──────────────┐              │  │
│  │  │ BUILD.bazel  │  │ CMakeLists   │              │  │
│  │  │             │  │     .txt     │              │  │
│  │  └──────────────┘  └──────────────┘              │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────────────┘
```

## 模块设计

### Version 模块

**文件**:
- `include/nano_redis/version.h` (公共 API)
- `src/version.cc` (实现)

**职责**:
- 提供版本信息查询
- 维护版本常量

**接口**:
```cpp
// 编译期常量
constexpr int kMajorVersion = 0;
constexpr int kMinorVersion = 1;
constexpr int kPatchVersion = 0;
constexpr const char* kVersionString = "0.1.0";

// 运行时函数
const char* GetVersion();
```

### Status 模块

**文件**:
- `include/nano_redis/status.h` (头文件实现)

**职责**:
- 统一的错误处理
- 错误信息传递

**接口**:
```cpp
// 错误码
enum class StatusCode {
  kOk = 0,
  kNotFound = 1,
  kInvalidArgument = 2,
  kInternalError = 3,
  kAlreadyExists = 4,
};

// Status 类
class Status {
  Status();                          // 默认 OK
  Status(StatusCode code, const std::string& message);

  static Status OK();
  static Status NotFound(const std::string& msg);
  static Status InvalidArgument(const std::string& msg);
  static Status Internal(const std::string& msg);
  static Status AlreadyExists(const std::string& msg);

  bool ok() const;
  StatusCode code() const;
  const std::string& message() const;
  std::string ToString() const;
};
```

## 依赖关系

```
version.h  ────────┐
                    ├─────> version_test.cc
version.cc  ────────┘

status.h   ─────────────> version_test.cc
```

## 数据流

### 版本查询流程

```
Client Code
     │
     │ GetVersion()
     ▼
version.h (inline function)
     │
     │ return kVersionString
     ▼
Client receives "0.1.0"
```

### 错误处理流程

```
Client Code
     │
     │ Status::NotFound("key not found")
     ▼
Status(code=kNotFound, message="key not found")
     │
     │ .ok() → false
     │ .code() → kNotFound
     │ .message() → "key not found"
     │ .ToString() → "NOT_FOUND: key not found"
     ▼
Error Handling
```

## 扩展点

### 后续模块可以依赖的基础

1. **Status**: 所有模块的错误处理
   ```cpp
   Status DoSomething() {
     if (error) {
       return Status::Internal("operation failed");
     }
     return Status::OK();
   }
   ```

2. **Version**: 版本检查和兼容性
   ```cpp
   if (client_version > kCurrentVersion) {
     return Status::InvalidArgument("version mismatch");
   }
   ```

## 编译模型

### 单元测试编译

```
CMake:
  version_test.cc (test) + version.cc (src) + GTest
          ↓
    version_test (executable)

Bazel:
  version_test.cc (test) + version.cc (src) + GTest
          ↓
    version_test (executable)
```

### 静态库编译

```
CMake:
  version.cc (src) + status.h (header)
          ↓
    nano_redis (static library)

Bazel:
  version.cc (src) + status.h (header)
          ↓
    nano_redis (static library)
```
