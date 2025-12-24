#ifndef NANO_REDIS_STRING_STORE_H_
#define NANO_REDIS_STRING_STORE_H_

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"

namespace nano_redis {

class StringStore {
 public:
  StringStore() = default;
  ~StringStore() = default;

  StringStore(const StringStore&) = delete;
  StringStore& operator=(const StringStore&) = delete;

  StringStore(StringStore&& other) noexcept = default;
  StringStore& operator=(StringStore&& other) noexcept = default;

  using StringMap = absl::flat_hash_map<std::string, std::string>;

  bool Put(const std::string& key, const std::string& value);
  bool Put(std::string&& key, std::string&& value);

  bool Get(const std::string& key, std::string* value) const;
  std::string* GetMutable(const std::string& key);

  bool Delete(const std::string& key);

  bool Contains(const std::string& key) const;

  size_t Size() const { return store_.size(); }
  bool Empty() const { return store_.empty(); }

  void Clear();

  size_t MemoryUsage() const;

  const StringMap& GetStore() const { return store_; }

 private:
  StringMap store_;
};

class StdStringStore {
 public:
  StdStringStore() = default;
  ~StdStringStore() = default;

  StdStringStore(const StdStringStore&) = delete;
  StdStringStore& operator=(const StdStringStore&) = delete;

  StdStringStore(StdStringStore&& other) noexcept = default;
  StdStringStore& operator=(StdStringStore&& other) noexcept = default;

  using StringMap = std::unordered_map<std::string, std::string>;

  bool Put(const std::string& key, const std::string& value);
  bool Put(std::string&& key, std::string&& value);

  bool Get(const std::string& key, std::string* value) const;
  std::string* GetMutable(const std::string& key);

  bool Delete(const std::string& key);

  bool Contains(const std::string& key) const;

  size_t Size() const { return store_.size(); }
  bool Empty() const { return store_.empty(); }

  void Clear();

  size_t MemoryUsage() const;

  const StringMap& GetStore() const { return store_; }

 private:
  StringMap store_;
};

}  // namespace nano_redis

#endif  // NANO_REDIS_STRING_STORE_H_
