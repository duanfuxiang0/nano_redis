#pragma once

#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include "core/nano_obj.h"

#include "core/dashtable.h"

class Database {
public:
	static constexpr size_t kNumDBs = 16;

	explicit Database();
	~Database() = default;

	bool Set(const NanoObj& key, const std::string& value);
	std::optional<std::string> Get(const NanoObj& key);
	bool Del(const NanoObj& key);
	bool Exists(const NanoObj& key);
	size_t KeyCount();
	bool Select(size_t db_index);
	size_t CurrentDB() const {
		return current_db;
	}
	void ClearCurrentDB();
	void ClearAll();
	std::vector<std::string> Keys();

	const NanoObj* Find(const NanoObj& key);
	bool Expire(const NanoObj& key, int64_t ttl_ms);
	bool Persist(const NanoObj& key);
	int64_t TTL(const NanoObj& key);
	size_t ActiveExpireCycle(size_t max_keys_per_db = 32);

	bool Set(const NanoObj& key, const NanoObj& value);
	bool Set(const NanoObj& key, NanoObj&& value);

	template <typename Func>
	void ForEachInDB(size_t db_index, Func&& func) const {
		if (db_index >= kNumDBs || tables[db_index] == nullptr) {
			return;
		}
		const int64_t now_ms = CurrentTimeMs();
		tables[db_index]->ForEach([db_index, now_ms, &func, this](const NanoObj& key, const NanoObj& value) {
			const int64_t* expire_at_ms = expire_tables[db_index]->Find(key);
			if (expire_at_ms != nullptr && *expire_at_ms <= now_ms) {
				return;
			}
			const int64_t expire_ms = (expire_at_ms == nullptr ? 0 : *expire_at_ms);
			func(key, value, expire_ms);
		});
	}

public:
	using Table = DashTable<NanoObj, NanoObj>;
	using ExpireTable = DashTable<NanoObj, int64_t>;

	Table* GetTable(size_t db_index) {
		if (db_index >= kNumDBs) {
			return nullptr;
		}
		return tables[db_index].get();
	}

	ExpireTable* GetExpireTable(size_t db_index) {
		if (db_index >= kNumDBs) {
			return nullptr;
		}
		return expire_tables[db_index].get();
	}

private:

	static int64_t CurrentTimeMs();
	bool IsExpiredInDB(size_t db_index, const NanoObj& key, int64_t now_ms) const;
	void PruneExpiredInDB(size_t db_index, const NanoObj& key, int64_t now_ms);

	std::array<std::unique_ptr<Table>, kNumDBs> tables;
	std::array<std::unique_ptr<ExpireTable>, kNumDBs> expire_tables;
	size_t current_db = 0;
};
