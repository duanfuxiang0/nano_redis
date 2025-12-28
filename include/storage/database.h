#pragma once

#include "storage/hashtable.h"
#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>

class Database {
public:
    static const size_t kNumDBs = 16;

    explicit Database();
    ~Database() = default;

    bool Set(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key) const;
    bool Del(const std::string& key);
    bool Exists(const std::string& key);
    size_t KeyCount() const;
    bool Select(size_t db_index);
    size_t CurrentDB() const { return current_db_; }
    void ClearCurrentDB();
    void ClearAll();
    std::vector<std::string> Keys() const;

private:
    using Table = HashTable<std::string, std::string>;
    std::array<std::unique_ptr<Table>, kNumDBs> tables_;
    size_t current_db_ = 0;
};
