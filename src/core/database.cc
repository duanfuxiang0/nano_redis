#include "core/database.h"

Database::Database() : current_db(0) {
	for (size_t i = 0; i < kNumDBs; ++i) {
		tables[i] = std::make_unique<DashTable<NanoObj, NanoObj>>();
	}
}

bool Database::Set(const NanoObj& key, const std::string& value) {
	return Set(key, NanoObj::FromKey(value));
}

std::optional<std::string> Database::Get(const NanoObj& key) const {
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
	return tables[current_db]->Erase(key);
}

bool Database::Exists(const NanoObj& key) {
	return tables[current_db]->Find(key) != nullptr;
}

size_t Database::KeyCount() const {
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
}

void Database::ClearAll() {
	for (size_t i = 0; i < kNumDBs; ++i) {
		tables[i]->Clear();
	}
}

std::vector<std::string> Database::Keys() const {
	std::vector<std::string> keys;
	tables[current_db]->ForEach([&keys](const NanoObj& key, const NanoObj& value) {
		(void)value;
		keys.push_back(key.ToString());
	});
	return keys;
}

const NanoObj* Database::Find(const NanoObj& key) const {
	return tables[current_db]->Find(key);
}
