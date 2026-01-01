#pragma once

#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>
#include "core/compact_obj.h"

#include "core/dashtable.h"

class Database {
public:
	static const size_t kNumDBs = 16;

	explicit Database();
	~Database() = default;

	bool Set(const CompactObj& key, const std::string& value);
	std::optional<std::string> Get(const CompactObj& key) const;
	bool Del(const CompactObj& key);
	bool Exists(const CompactObj& key);
	size_t KeyCount() const;
	bool Select(size_t db_index);
	size_t CurrentDB() const {
		return current_db_;
	}
	void ClearCurrentDB();
	void ClearAll();
	std::vector<std::string> Keys() const;

	bool Set(const CompactObj& key, const CompactObj& value);
	bool Set(const CompactObj& key, CompactObj&& value);

private:
	using Table = DashTable<CompactObj, CompactObj>;
	std::array<std::unique_ptr<Table>, kNumDBs> tables_;
	size_t current_db_ = 0;
};
