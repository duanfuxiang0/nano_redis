#pragma once

#include <cstdint>
#include <system_error>
#include <atomic>

#include "core/database.h"
#include "core/rdb_serializer.h"

class SliceSnapshot {
public:
	SliceSnapshot(Database* db, RdbSerializer* serializer, uint64_t snapshot_version);
	~SliceSnapshot();

	SliceSnapshot(const SliceSnapshot&) = delete;
	SliceSnapshot& operator=(const SliceSnapshot&) = delete;

public:
	std::error_code SerializeAllDBs();
	bool HasError() const {
		return static_cast<bool>(error_);
	}
	std::error_code GetError() const {
		return error_;
	}

private:
	std::error_code SerializeDB(size_t db_index);
	void SerializeSegment(size_t db_index, size_t dir_idx);
	void OnPreModify(size_t db_index, size_t dir_idx);

private:
	Database* db_;
	RdbSerializer* serializer_;
	uint64_t snapshot_version_;
	std::error_code error_;
};
