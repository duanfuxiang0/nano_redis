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

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

namespace nano_redis {

// 测试 EchoServer 的基本功能
TEST(EchoServerTest, ConstructionAndDestruction) {
  EchoServer server;
  // 服务器可以正常构造和析构
}

// 测试启动和停止服务器
TEST(EchoServerTest, StartAndStop) {
  EchoServer server;
  
  // 启动服务器
  ASSERT_EQ(0, server.Start(18080)) << "Failed to start echo server";
  
  // 等待一小段时间确保服务器启动
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  // 停止服务器
  server.Stop();
}

// 测试多次启动会失败
TEST(EchoServerTest, StartMultipleTimes) {
  EchoServer server;
  
  // 第一次启动应该成功
  ASSERT_EQ(0, server.Start(18081)) << "First start should succeed";
  
  // 等待服务器启动
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  // 第二次启动应该失败
  ASSERT_NE(0, server.Start(18082)) << "Second start should fail";
  
  // 停止服务器
  server.Stop();
}

// 测试服务器在特定端口启动
TEST(EchoServerTest, StartOnDifferentPorts) {
  std::vector<uint16_t> ports = {18090, 18091, 18092};
  
  for (auto port : ports) {
    EchoServer server;
    EXPECT_EQ(0, server.Start(port)) << "Failed to start on port " << port;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    server.Stop();
  }
}

// 测试顺序运行多个服务器（PhotonLibOS 全局单例限制）
TEST(EchoServerTest, SequentialServers) {
  // PhotonLibOS 是全局单例，不能同时运行多个实例
  // 但可以按顺序启动/停止多个服务器
  
  EchoServer server1;
  ASSERT_EQ(0, server1.Start(18100)) << "Server 1 start failed";
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server1.Stop();
  
  // 等待 PhotonLibOS 完全清理
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  EchoServer server2;
  ASSERT_EQ(0, server2.Start(18101)) << "Server 2 start failed";
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server2.Stop();
}

// 测试 Stop 可以安全地被调用多次
TEST(EchoServerTest, StopMultipleTimes) {
  EchoServer server;
  
  // 启动服务器
  ASSERT_EQ(0, server.Start(18110));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  // 多次调用 Stop 不应该崩溃
  server.Stop();
  server.Stop();
  server.Stop();
}

}  // namespace nano_redis
