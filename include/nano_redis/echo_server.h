// Copyright 2025 Nano-Redis Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NANO_REDIS_ECHO_SERVER_H_
#define NANO_REDIS_ECHO_SERVER_H_

#include <cstdint>

namespace nano_redis {

// EchoServer 实现了一个简单的 Echo 服务器
// 使用 PhotonLibOS 协程模型，每个连接在独立的协程中处理
//
// 设计原则：
// 1. 协程并发：每个连接独立协程，互不阻塞
// 2. 同步写法：使用 sock->read()/write()，异步执行
// 3. 资源管理：连接关闭时自动释放资源
class EchoServer {
 public:
  EchoServer();
  ~EchoServer();

  EchoServer(const EchoServer&) = delete;
  EchoServer& operator=(const EchoServer&) = delete;

  // 启动 Echo 服务器
  // port: 监听端口
  // 返回: 0 成功, -1 失败
  int Start(uint16_t port);

  // 停止服务器
  void Stop();

 private:
  class Impl;
  Impl* impl_;  // Pimpl 模式，隐藏 PhotonLibOS 依赖
};

}  // namespace nano_redis

#endif  // NANO_REDIS_ECHO_SERVER_H_
