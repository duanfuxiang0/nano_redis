#pragma once

#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>
#include "core/nano_obj.h"

#include "core/dashtable.h"

class Database {
public:
	static const size_t kNumDBs = 16;

	explicit Database();
	~Database() = default;

	bool Set(const NanoObj& key, const std::string& value);
	std::optional<std::string> Get(const NanoObj& key) const;
	bool Del(const NanoObj& key);
	bool Exists(const NanoObj& key);
	size_t KeyCount() const;
	bool Select(size_t db_index);
	size_t CurrentDB() const {
		return current_db_;
	}
	void ClearCurrentDB();
	void ClearAll();
	std::vector<std::string> Keys() const;

	const NanoObj* Find(const NanoObj& key) const;

	bool Set(const NanoObj& key, const NanoObj& value);
	bool Set(const NanoObj& key, NanoObj&& value);

private:
	using Table = DashTable<NanoObj, NanoObj>;
	std::array<std::unique_ptr<Table>, kNumDBs> tables_;
	size_t current_db_ = 0;
};
