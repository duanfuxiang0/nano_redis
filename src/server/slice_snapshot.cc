#include "server/slice_snapshot.h"

#include <chrono>

namespace {

int64_t CurrentTimeMs() {
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::milliseconds;
	return std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
}

}  // namespace

SliceSnapshot::SliceSnapshot(Database* db, RdbSerializer* serializer, uint64_t snapshot_version)
    : db_(db), serializer_(serializer), snapshot_version_(snapshot_version) {}

SliceSnapshot::~SliceSnapshot() {
	for (size_t db_index = 0; db_index < Database::kNumDBs; ++db_index) {
		auto* table = db_->GetTable(db_index);
		if (table != nullptr) {
			table->ClearPreModifyCallback();
		}
	}
}

std::error_code SliceSnapshot::SerializeAllDBs() {
	for (size_t db_index = 0; db_index < Database::kNumDBs; ++db_index) {
		auto ec = SerializeDB(db_index);
		if (ec) {
			error_ = ec;
			return ec;
		}
	}
	return {};
}

std::error_code SliceSnapshot::SerializeDB(size_t db_index) {
	auto* table = db_->GetTable(db_index);
	if (table == nullptr || table->Size() == 0) {
		return {};
	}

	table->SetPreModifyCallback([this, db_index](size_t dir_idx) {
		OnPreModify(db_index, dir_idx);
	});

	for (size_t dir_idx = 0; dir_idx < table->DirSize();
	     dir_idx = table->NextUniqueSegment(dir_idx)) {
		if (error_) {
			break;
		}
		if (table->GetSegVersion(dir_idx) < snapshot_version_) {
			SerializeSegment(db_index, dir_idx);
			table->SetSegVersion(dir_idx, snapshot_version_);
		}
	}

	table->ClearPreModifyCallback();
	return error_;
}

void SliceSnapshot::SerializeSegment(size_t db_index, size_t dir_idx) {
	if (error_) {
		return;
	}

	auto* table = db_->GetTable(db_index);
	if (table == nullptr) {
		return;
	}

	const auto* expire_table = db_->GetExpireTable(db_index);
	const int64_t now_ms = CurrentTimeMs();
	const uint32_t dbid = static_cast<uint32_t>(db_index);

	table->ForEachInSeg(dir_idx, [&](const NanoObj& key, const NanoObj& value) {
		if (error_) {
			return;
		}
		int64_t expire_ms = 0;
		if (expire_table != nullptr) {
			const int64_t* exp = expire_table->Find(key);
			if (exp != nullptr) {
				if (*exp <= now_ms) {
					return;
				}
				expire_ms = *exp;
			}
		}
		error_ = serializer_->SaveEntry(key, value, expire_ms, dbid);
	});
}

void SliceSnapshot::OnPreModify(size_t db_index, size_t dir_idx) {
	auto* table = db_->GetTable(db_index);
	if (table == nullptr) {
		return;
	}

	if (table->GetSegVersion(dir_idx) < snapshot_version_) {
		SerializeSegment(db_index, dir_idx);
		table->SetSegVersion(dir_idx, snapshot_version_);
	}
}
