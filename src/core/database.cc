#include "core/database.h"

#include <chrono>
#include <limits>

Database::Database() : current_db(0) {
	for (size_t i = 0; i < kNumDBs; ++i) {
		tables[i] = std::make_unique<DashTable<NanoObj, NanoObj>>();
		expire_tables[i] = std::make_unique<ExpireTable>();
	}
}

bool Database::Set(const NanoObj& key, const std::string& value) {
	return Set(key, NanoObj::FromKey(value));
}

std::optional<std::string> Database::Get(const NanoObj& key) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);

	const NanoObj* val = tables[current_db]->Find(key);
	if (val) {
		return val->ToString();
	}
	return std::nullopt;
}

bool Database::Set(const NanoObj& key, NanoObj&& value) {
	tables[current_db]->Insert(key, std::move(value));
	return true;
}

bool Database::Set(const NanoObj& key, const NanoObj& value) {
	tables[current_db]->Insert(key, value);
	return true;
}

bool Database::Del(const NanoObj& key) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);

	const bool deleted = tables[current_db]->Erase(key);
	if (deleted) {
		(void)expire_tables[current_db]->Erase(key);
	}
	return deleted;
}

bool Database::Exists(const NanoObj& key) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);
	return tables[current_db]->Find(key) != nullptr;
}

size_t Database::KeyCount() {
	const int64_t now_ms = CurrentTimeMs();
	std::vector<NanoObj> expired_keys;
	expire_tables[current_db]->ForEach([&](const NanoObj& key, const int64_t& expire_at_ms) {
		if (expire_at_ms <= now_ms) {
			expired_keys.push_back(key);
		}
	});

	for (const auto& key : expired_keys) {
		(void)tables[current_db]->Erase(key);
		(void)expire_tables[current_db]->Erase(key);
	}

	return tables[current_db]->Size();
}

bool Database::Select(size_t db_index) {
	if (db_index >= kNumDBs) {
		return false;
	}
	current_db = db_index;
	return true;
}

void Database::ClearCurrentDB() {
	tables[current_db]->Clear();
	expire_tables[current_db]->Clear();
}

void Database::ClearAll() {
	for (size_t i = 0; i < kNumDBs; ++i) {
		tables[i]->Clear();
		expire_tables[i]->Clear();
	}
}

std::vector<std::string> Database::Keys() {
	const int64_t now_ms = CurrentTimeMs();
	std::vector<NanoObj> expired_keys;
	expire_tables[current_db]->ForEach([&](const NanoObj& key, const int64_t& expire_at_ms) {
		if (expire_at_ms <= now_ms) {
			expired_keys.push_back(key);
		}
	});
	for (const auto& key : expired_keys) {
		(void)tables[current_db]->Erase(key);
		(void)expire_tables[current_db]->Erase(key);
	}

	std::vector<std::string> keys;
	tables[current_db]->ForEach([&keys](const NanoObj& key, const NanoObj& value) {
		(void)value;
		keys.push_back(key.ToString());
	});
	return keys;
}

const NanoObj* Database::Find(const NanoObj& key) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);
	return tables[current_db]->Find(key);
}

bool Database::Expire(const NanoObj& key, int64_t ttl_ms) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);

	if (tables[current_db]->Find(key) == nullptr) {
		return false;
	}

	if (ttl_ms <= 0) {
		(void)tables[current_db]->Erase(key);
		(void)expire_tables[current_db]->Erase(key);
		return true;
	}

	int64_t expire_at_ms = now_ms;
	if (ttl_ms >= std::numeric_limits<int64_t>::max() - now_ms) {
		expire_at_ms = std::numeric_limits<int64_t>::max();
	} else {
		expire_at_ms += ttl_ms;
	}
	expire_tables[current_db]->Insert(key, expire_at_ms);
	return true;
}

bool Database::Persist(const NanoObj& key) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);

	if (tables[current_db]->Find(key) == nullptr) {
		return false;
	}
	return expire_tables[current_db]->Erase(key);
}

int64_t Database::TTL(const NanoObj& key) {
	const int64_t now_ms = CurrentTimeMs();
	PruneExpiredInDB(current_db, key, now_ms);

	if (tables[current_db]->Find(key) == nullptr) {
		return -2;
	}

	const int64_t* expire_at_ms = expire_tables[current_db]->Find(key);
	if (expire_at_ms == nullptr) {
		return -1;
	}

	if (*expire_at_ms <= now_ms) {
		(void)tables[current_db]->Erase(key);
		(void)expire_tables[current_db]->Erase(key);
		return -2;
	}

	const int64_t remaining_ms = *expire_at_ms - now_ms;
	return remaining_ms / 1000;
}

size_t Database::ActiveExpireCycle(size_t max_keys_per_db) {
	if (max_keys_per_db == 0) {
		return 0;
	}

	const int64_t now_ms = CurrentTimeMs();
	size_t deleted_count = 0;

	for (size_t db_index = 0; db_index < kNumDBs; ++db_index) {
		size_t scanned = 0;
		std::vector<NanoObj> expired_keys;
		expire_tables[db_index]->ForEach([&](const NanoObj& key, const int64_t& expire_at_ms) {
			if (scanned >= max_keys_per_db) {
				return;
			}
			++scanned;
			if (expire_at_ms <= now_ms) {
				expired_keys.push_back(key);
			}
		});

		for (const auto& key : expired_keys) {
			if (tables[db_index]->Erase(key)) {
				++deleted_count;
			}
			(void)expire_tables[db_index]->Erase(key);
		}
	}

	return deleted_count;
}

int64_t Database::CurrentTimeMs() {
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::milliseconds;
	return std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
}

bool Database::IsExpiredInDB(size_t db_index, const NanoObj& key, int64_t now_ms) const {
	const int64_t* expire_at_ms = expire_tables[db_index]->Find(key);
	return expire_at_ms != nullptr && *expire_at_ms <= now_ms;
}

void Database::PruneExpiredInDB(size_t db_index, const NanoObj& key, int64_t now_ms) {
	if (!IsExpiredInDB(db_index, key, now_ms)) {
		return;
	}

	(void)tables[db_index]->Erase(key);
	(void)expire_tables[db_index]->Erase(key);
}
