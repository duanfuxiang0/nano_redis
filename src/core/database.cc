#include "core/database.h"

Database::Database() : current_db_(0) {
	for (size_t i = 0; i < kNumDBs; ++i) {
		tables_[i] = std::make_unique<DashTable<NanoObj, NanoObj>>();
	}
}

bool Database::Set(const NanoObj& key, const std::string& value) {
	return Set(key, std::move(NanoObj::fromKey(value)));
}

std::optional<std::string> Database::Get(const NanoObj& key) const {
	const NanoObj* val = tables_[current_db_]->Find(key);
	if (val) {
		return val->toString();
	}
	return std::nullopt;
}

 bool Database::Set(const NanoObj& key, NanoObj&& value) {
	tables_[current_db_]->Insert(key, std::move(value));
	return true;
}

 bool Database::Set(const NanoObj& key, const NanoObj& value) {
	tables_[current_db_]->Insert(key, value);
	return true;
}

bool Database::Del(const NanoObj& key) {
	return tables_[current_db_]->Erase(key);
}

bool Database::Exists(const NanoObj& key) {
	return tables_[current_db_]->Find(key) != nullptr;
}

size_t Database::KeyCount() const {
	return tables_[current_db_]->Size();
}

bool Database::Select(size_t db_index) {
	if (db_index >= kNumDBs) {
		return false;
	}
	current_db_ = db_index;
	return true;
}

void Database::ClearCurrentDB() {
	tables_[current_db_]->Clear();
}

void Database::ClearAll() {
	for (size_t i = 0; i < kNumDBs; ++i) {
		tables_[i]->Clear();
	}
}

std::vector<std::string> Database::Keys() const {
	std::vector<std::string> keys;
	tables_[current_db_]->ForEach([&keys](const NanoObj& key, const NanoObj& value) {
		(void)value;
		keys.push_back(key.toString());
	});
	return keys;
}

const NanoObj* Database::Find(const NanoObj& key) const {
	return tables_[current_db_]->Find(key);
}
