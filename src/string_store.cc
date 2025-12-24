#include "nano_redis/string_store.h"

#include <string>

namespace nano_redis {

bool StringStore::Put(const std::string& key, const std::string& value) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    it->second = value;
  } else {
    store_.insert({key, value});
  }
  return true;
}

bool StringStore::Put(std::string&& key, std::string&& value) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    it->second = std::move(value);
  } else {
    store_.insert({std::move(key), std::move(value)});
  }
  return true;
}

bool StringStore::Get(const std::string& key, std::string* value) const {
  auto it = store_.find(key);
  if (it != store_.end()) {
    if (value != nullptr) {
      *value = it->second;
    }
    return true;
  }
  return false;
}

std::string* StringStore::GetMutable(const std::string& key) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    return &it->second;
  }
  return nullptr;
}

bool StringStore::Delete(const std::string& key) {
  return store_.erase(key) > 0;
}

bool StringStore::Contains(const std::string& key) const {
  return store_.contains(key);
}

void StringStore::Clear() {
  store_.clear();
}

size_t StringStore::MemoryUsage() const {
  size_t usage = sizeof(StringMap);
  usage += store_.capacity() * (sizeof(std::string) * 2 + sizeof(void*));
  for (const auto& [key, value] : store_) {
    usage += key.capacity();
    usage += value.capacity();
  }
  return usage;
}

bool StdStringStore::Put(const std::string& key, const std::string& value) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    it->second = value;
  } else {
    store_.insert({key, value});
  }
  return true;
}

bool StdStringStore::Put(std::string&& key, std::string&& value) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    it->second = std::move(value);
  } else {
    store_.insert({std::move(key), std::move(value)});
  }
  return true;
}

bool StdStringStore::Get(const std::string& key, std::string* value) const {
  auto it = store_.find(key);
  if (it != store_.end()) {
    if (value != nullptr) {
      *value = it->second;
    }
    return true;
  }
  return false;
}

std::string* StdStringStore::GetMutable(const std::string& key) {
  auto it = store_.find(key);
  if (it != store_.end()) {
    return &it->second;
  }
  return nullptr;
}

bool StdStringStore::Delete(const std::string& key) {
  return store_.erase(key) > 0;
}

bool StdStringStore::Contains(const std::string& key) const {
  return store_.find(key) != store_.end();
}

void StdStringStore::Clear() {
  store_.clear();
}

size_t StdStringStore::MemoryUsage() const {
  size_t usage = sizeof(StringMap);
  usage += store_.bucket_count() * (sizeof(std::string) * 2 + sizeof(void*));
  for (const auto& [key, value] : store_) {
    usage += key.capacity();
    usage += value.capacity();
  }
  return usage;
}

}  // namespace nano_redis
