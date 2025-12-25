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

#include "nano_redis/echo_server.h"

#include <photon/photon.h>
#include <photon/net/socket.h>

#include <iostream>
#include <memory>

namespace nano_redis {

// Pimpl 实现类，隐藏 PhotonLibOS 依赖
class EchoServer::Impl {
 public:
  Impl() : server_(nullptr), running_(false) {}

  ~Impl() {
    Stop();
  }

  int Start(uint16_t port) {
    if (running_) {
      std::cerr << "Server is already running" << std::endl;
      return -1;
    }

    // 初始化 PhotonLibOS，使用 epoll 后端
    if (photon::init(photon::INIT_EVENT_EPOLL, photon::INIT_IO_NONE)) {
      std::cerr << "Failed to initialize PhotonLibOS" << std::endl;
      return -1;
    }

    // 创建 TCP 服务器
    server_ = photon::net::new_tcp_socket_server();
    if (!server_) {
      std::cerr << "Failed to create TCP server" << std::endl;
      photon::fini();
      return -1;
    }

    // 设置连接处理器
    server_->set_handler(HandleConnection);
    server_->timeout(1000000);  // 1 秒超时

    // 绑定端口
    if (server_->bind_v4any(port) != 0) {
      std::cerr << "Failed to bind port " << port << std::endl;
      delete server_;
      server_ = nullptr;
      photon::fini();
      return -1;
    }

    // 开始监听
    if (server_->listen(1024) != 0) {
      std::cerr << "Failed to listen" << std::endl;
      delete server_;
      server_ = nullptr;
      photon::fini();
      return -1;
    }

    // 启动服务器循环
    if (server_->start_loop(false) != 0) {
      std::cerr << "Failed to start server loop" << std::endl;
      delete server_;
      server_ = nullptr;
      photon::fini();
      return -1;
    }

    running_ = true;
    std::cout << "Echo server started on port " << port << std::endl;
    return 0;
  }

  void Stop() {
    if (!running_) {
      return;
    }

    if (server_) {
      server_->terminate();
      delete server_;
      server_ = nullptr;
    }

    photon::fini();
    running_ = false;
    std::cout << "Echo server stopped" << std::endl;
  }

 private:
  // 处理单个连接
  // 这个函数在独立的协程中运行
  // 返回 int 以匹配 PhotonLibOS 的 Delegate 签名
  static int HandleConnection(photon::net::ISocketStream* sock) {
    if (!sock) {
      return 0;
    }

    constexpr size_t kBufferSize = 4096;
    char buffer[kBufferSize];

    while (true) {
      // 协程安全读取：自动 yield，不阻塞其他协程
      ssize_t n = sock->read(buffer, kBufferSize);
      
      if (n <= 0) {
        // 连接关闭或错误
        break;
      }

      // 回显数据
      ssize_t written = sock->write(buffer, n);
      if (written != n) {
        std::cerr << "Write error: written " << written << " of " << n
                  << std::endl;
        break;
      }
    }

    // PhotonLibOS 需要手动释放连接
    delete sock;
    return 0;  // 返回成功
  }

  photon::net::ISocketServer* server_;
  bool running_;
};

EchoServer::EchoServer() : impl_(new Impl()) {}

EchoServer::~EchoServer() {
  delete impl_;
}

int EchoServer::Start(uint16_t port) {
  return impl_->Start(port);
}

void EchoServer::Stop() {
  impl_->Stop();
}

}  // namespace nano_redis
