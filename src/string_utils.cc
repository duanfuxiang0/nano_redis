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

#include "nano_redis/string_utils.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace nano_redis {

bool StringUtils::StartsWithIgnoreCase(const std::string& s,
                                        const std::string& prefix) {
  if (prefix.size() > s.size()) {
    return false;
  }

  // 遍历比较每个字符（不区分大小写）
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (ToLowerChar(s[i]) != ToLowerChar(prefix[i])) {
      return false;
    }
  }

  return true;
}

bool StringUtils::EndsWithIgnoreCase(const std::string& s,
                                       const std::string& suffix) {
  if (suffix.size() > s.size()) {
    return false;
  }

  size_t start_pos = s.size() - suffix.size();

  // 遍历比较每个字符（不区分大小写）
  for (size_t i = 0; i < suffix.size(); ++i) {
    if (ToLowerChar(s[start_pos + i]) != ToLowerChar(suffix[i])) {
      return false;
    }
  }

  return true;
}

void StringUtils::ToLower(std::string* s) {
  if (s == nullptr || s->empty()) {
    return;
  }

  // 原地修改，避免创建新字符串
  for (char& c : *s) {
    c = ToLowerChar(c);
  }
}

std::string StringUtils::ToLowerCopy(std::string s) {
  // 使用 std::move 避免 unnecessary copy
  ToLower(&s);
  return s;  // NRVO（Named Return Value Optimization）
}

void StringUtils::ToUpper(std::string* s) {
  if (s == nullptr || s->empty()) {
    return;
  }

  // 原地修改
  for (char& c : *s) {
    c = ToUpperChar(c);
  }
}

std::string StringUtils::ToUpperCopy(std::string s) {
  ToUpper(&s);
  return s;
}

void StringUtils::TrimLeft(std::string* s) {
  if (s == nullptr || s->empty()) {
    return;
  }

  // 找到第一个非空白字符
  size_t start = 0;
  while (start < s->size() && std::isspace(static_cast<unsigned char>((*s)[start]))) {
    ++start;
  }

  if (start > 0) {
    // 使用 erase 删除前导空白
    // erase 可能会移动剩余字符，但对于 SSO 字符串来说影响很小
    s->erase(0, start);
  }
}

void StringUtils::TrimRight(std::string* s) {
  if (s == nullptr || s->empty()) {
    return;
  }

  // 找到最后一个非空白字符
  size_t end = s->size();
  while (end > 0 && std::isspace(static_cast<unsigned char>((*s)[end - 1]))) {
    --end;
  }

  if (end < s->size()) {
    // resize 减少字符串长度，不触发重新分配
    s->resize(end);
  }
}

void StringUtils::Trim(std::string* s) {
  TrimRight(s);
  TrimLeft(s);
}

std::vector<std::string> StringUtils::Split(const std::string& s, char delim) {
  std::vector<std::string> result;

  if (s.empty()) {
    return result;
  }

  // 预估结果大小，减少重新分配
  size_t count = std::count(s.begin(), s.end(), delim) + 1;
  result.reserve(count);

  size_t start = 0;
  size_t pos = s.find(delim);

  while (pos != std::string::npos) {
    // 使用 substr 创建子字符串
    // 对于小字符串会使用 SSO，无堆分配
    result.push_back(s.substr(start, pos - start));
    start = pos + 1;
    pos = s.find(delim, start);
  }

  // 添加最后一个部分
  result.push_back(s.substr(start));

  return result;  // RVO 优化
}

std::string StringUtils::Join(const std::vector<std::string>& parts,
                               const std::string& delimiter) {
  if (parts.empty()) {
    return "";
  }

  // 计算最终大小，预分配内存
  size_t total_size = 0;
  for (const auto& part : parts) {
    total_size += part.size();
  }
  total_size += delimiter.size() * (parts.size() - 1);

  std::string result;
  result.reserve(total_size);

  // 使用 operator+= 追加字符串
  // 由于已经 reserve，不会触发重新分配
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      result += delimiter;
    }
    result += parts[i];
  }

  return result;
}

std::string StringUtils::IntToString(int64_t value, int base) {
  if (base != 10) {
    // 非 10 进制使用 std::to_string + 手动转换
    if (base == 16) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(value));
      return std::string(buf);
    }
    // 其他进制可以扩展
  }

  // 使用 std::to_string（优化版本，比 sprintf 快）
  return std::to_string(value);
}

bool StringUtils::StringToInt(const std::string& s, int64_t* value) {
  if (s.empty()) {
    return false;
  }

  char* end = nullptr;
  int64_t result = std::strtoll(s.c_str(), &end, 10);

  // 检查是否整个字符串都被解析
  if (end == nullptr || *end != '\0') {
    return false;
  }

  if (value != nullptr) {
    *value = result;
  }

  return true;
}

std::string StringUtils::Escape(const std::string& s) {
  std::string result;
  result.reserve(s.size() * 2);  // 预估大小

  for (char c : s) {
    switch (c) {
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '"':
        result += "\\\"";
        break;
      default:
        result += c;
    }
  }

  return result;
}

std::string StringUtils::Unescape(const std::string& s) {
  std::string result;
  result.reserve(s.size());

  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      switch (s[i + 1]) {
        case 'n':
          result += '\n';
          ++i;
          break;
        case 'r':
          result += '\r';
          ++i;
          break;
        case 't':
          result += '\t';
          ++i;
          break;
        case '\\':
          result += '\\';
          ++i;
          break;
        case '"':
          result += '"';
          ++i;
          break;
        default:
          result += s[i];  // 保留反斜杠
      }
    } else {
      result += s[i];
    }
  }

  return result;
}

std::string StringUtils::Concat(const std::vector<std::string>& parts) {
  if (parts.empty()) {
    return "";
  }

  // 计算总大小
  size_t total_size = 0;
  for (const auto& part : parts) {
    total_size += part.size();
  }

  std::string result;
  result.reserve(total_size);

  // 使用 append 比 += 稍快（避免额外检查）
  for (const auto& part : parts) {
    result.append(part);
  }

  return result;
}

int StringUtils::CompareIgnoreCase(const std::string& a, const std::string& b) {
  size_t min_len = std::min(a.size(), b.size());

  for (size_t i = 0; i < min_len; ++i) {
    char ca = ToLowerChar(a[i]);
    char cb = ToLowerChar(b[i]);

    if (ca != cb) {
      return (ca < cb) ? -1 : 1;
    }
  }

  // 较长的字符串更大
  if (a.size() < b.size()) {
    return -1;
  } else if (a.size() > b.size()) {
    return 1;
  }

  return 0;
}

size_t StringUtils::FindIgnoreCase(const std::string& haystack,
                                    const std::string& needle,
                                    size_t pos) {
  if (needle.empty()) {
    return pos;
  }

  if (pos >= haystack.size() || needle.size() > haystack.size() - pos) {
    return std::string::npos;
  }

  // 遍历 haystack，逐个位置检查
  for (size_t i = pos; i <= haystack.size() - needle.size(); ++i) {
    bool match = true;

    for (size_t j = 0; j < needle.size(); ++j) {
      if (ToLowerChar(haystack[i + j]) != ToLowerChar(needle[j])) {
        match = false;
        break;
      }
    }

    if (match) {
      return i;
    }
  }

  return std::string::npos;
}

std::string StringUtils::ReplaceAll(std::string s,
                                     const std::string& from,
                                     const std::string& to) {
  if (from.empty()) {
    return s;
  }

  size_t pos = 0;
  size_t from_len = from.size();
  size_t to_len = to.size();

  // 预估可能的大小变化
  size_t count = std::count(s.begin(), s.end(), from[0]);
  if (count > 0) {
    s.reserve(s.size() + count * (to_len - from_len));
  }

  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from_len, to);
    pos += to_len;  // 跳过替换后的字符串
  }

  return s;
}

}  // namespace nano_redis
