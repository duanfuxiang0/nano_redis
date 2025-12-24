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

#ifndef NANO_REDIS_STRING_UTILS_H_
#define NANO_REDIS_STRING_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nano_redis {

// StringUtils 提供高效的字符串操作工具函数
//
// 设计原则：
// 1. 避免不必要的拷贝（使用 std::move）
// 2. 引用传递避免拷贝（const std::string&）
// 3. 利用 SSO（Small String Optimization）
// 4. 零拷贝操作（尽可能避免临时字符串）
class StringUtils {
 public:
  // 判断字符串是否为空
  // 使用 empty() 而不是 size() == 0，更清晰语义
  static bool IsEmpty(const std::string& s) { return s.empty(); }

  // 检查字符串是否以指定前缀开头（不区分大小写）
  // 使用 const std::string& 避免拷贝
  static bool StartsWithIgnoreCase(const std::string& s,
                                    const std::string& prefix);

  // 检查字符串是否以指定后缀结尾（不区分大小写）
  static bool EndsWithIgnoreCase(const std::string& s,
                                 const std::string& suffix);

  // 转换为小写（原地修改）
  // 避免创建新字符串，提高效率
  static void ToLower(std::string* s);

  // 转换为小写（返回新字符串）
  // 使用 std::move 避免不必要的拷贝
  static std::string ToLowerCopy(std::string s);

  // 转换为大写（原地修改）
  static void ToUpper(std::string* s);

  // 转换为大写（返回新字符串）
  static std::string ToUpperCopy(std::string s);

  // 去除前导空白字符
  static void TrimLeft(std::string* s);

  // 去除尾部空白字符
  static void TrimRight(std::string* s);

  // 去除两端空白字符
  static void Trim(std::string* s);

  // 分割字符串
  // 返回 vector<string>，利用 RVO 优化
  static std::vector<std::string> Split(const std::string& s, char delim);

  // 连接字符串
  // 使用 reserve 减少重新分配
  static std::string Join(const std::vector<std::string>& parts,
                          const std::string& delimiter);

  // 格式化数字为字符串
  // 比 std::to_string 更灵活（支持不同进制）
  static std::string IntToString(int64_t value, int base = 10);

  // 解析字符串为整数
  static bool StringToInt(const std::string& s, int64_t* value);

  // 转义特殊字符（用于 RESP 协议）
  static std::string Escape(const std::string& s);

  // 反转义特殊字符
  static std::string Unescape(const std::string& s);

  // 计算 SSO（Small String Optimization）阈值
  // 展示 std::string 的优化特性
  static size_t SSOThreshold() {
    // GCC/Clang 默认 SSO 阈值为 15（不包含 '\0'）
    // 使用 std::string 的内部特性
    std::string tmp;
    return tmp.capacity();  // 空字符串的 capacity 就是 SSO 阈值
  }

  // 检查字符串是否使用了 SSO
  static bool IsSSO(const std::string& s) {
    return s.size() <= SSOThreshold();
  }

  // 高效字符串拼接
  // 使用 reserve 预分配内存，避免多次重新分配
  static std::string Concat(const std::vector<std::string>& parts);

  // 字符串比较（忽略大小写）
  static int CompareIgnoreCase(const std::string& a, const std::string& b);

  // 查找子字符串（不区分大小写）
  static size_t FindIgnoreCase(const std::string& haystack,
                               const std::string& needle,
                               size_t pos = 0);

  // 替换所有出现的子字符串
  // 使用迭代器避免多次内存分配
  static std::string ReplaceAll(std::string s,
                                const std::string& from,
                                const std::string& to);

 private:
  // 字符转小写
  static char ToLowerChar(char c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
  }

  // 字符转大写
  static char ToUpperChar(char c) {
    return (c >= 'a' && c <= 'z') ? (c - 32) : c;
  }
};

}  // namespace nano_redis

#endif  // NANO_REDIS_STRING_UTILS_H_
