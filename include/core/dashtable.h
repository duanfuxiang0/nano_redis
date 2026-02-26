#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>

#include "unordered_dense.h"

namespace {
constexpr uint64_t kInitialSegmentCount = 1;
constexpr uint64_t kDefaultFixedBucketCount = 16;
constexpr float kSplitThreshold = 0.8f;
} // namespace

template <typename K, typename V>
class DashTable {
public:
	using PreModifyCallback = std::function<void(size_t dir_idx)>;

	explicit DashTable(uint64_t initial_segment_count = kInitialSegmentCount,
	                   uint64_t max_segment_size = kDefaultFixedBucketCount);

	~DashTable();

	DashTable(const DashTable&) = delete;
	DashTable& operator=(const DashTable&) = delete;

	DashTable(DashTable&& other) noexcept;
	DashTable& operator=(DashTable&& other) noexcept;

	void Insert(const K& key, V&& value);
	void Insert(const K& key, const V& value);
	const V* Find(const K& key) const;
	bool Erase(const K& key);
	void Clear();

	uint64_t Size() const;
	uint64_t SegmentCount() const;
	uint64_t BucketCount() const;

	template <typename FUNC>
	void ForEach(FUNC&& func) const {
		for (size_t i = 0; i < segment_directory.size(); i = NextSeg(i)) {
			const auto& segment = segment_directory[i];
			for (const auto& [key, value] : segment->table) {
				func(key, value);
			}
		}
	}

	template <typename FUNC>
	void ForEachInSeg(size_t dir_idx, FUNC&& func) const {
		if (dir_idx >= segment_directory.size()) {
			return;
		}
		for (const auto& [key, value] : segment_directory[dir_idx]->table) {
			func(key, value);
		}
	}

	void SetPreModifyCallback(PreModifyCallback cb) {
		pre_modify_cb_ = std::move(cb);
	}

	void ClearPreModifyCallback() {
		pre_modify_cb_ = nullptr;
	}

	size_t DirSize() const {
		return segment_directory.size();
	}

	uint64_t GetSegVersion(size_t dir_idx) const {
		return segment_directory[dir_idx]->version;
	}

	void SetSegVersion(size_t dir_idx, uint64_t ver) {
		segment_directory[dir_idx]->version = ver;
	}

	size_t NextUniqueSegment(size_t sid) const {
		return NextSeg(sid);
	}

private:
	struct Segment {
		ankerl::unordered_dense::map<K, V, ankerl::unordered_dense::hash<K>> table;
		uint8_t local_depth;
		uint32_t segment_id;
		uint64_t version = 0;

		explicit Segment(uint8_t depth, uint32_t id, uint64_t fixed_bucket_count)
		    : table(fixed_bucket_count), local_depth(depth), segment_id(id) {
			table.max_load_factor(1.0f);
		}
	};

	uint64_t GetSegmentIndex(const K& key) const;
	void SplitSegment(uint32_t seg_id);
	bool NeedSplit(uint32_t seg_id) const;
	size_t NextSeg(size_t sid) const;

	// Extendible hashing directory slots can alias the same segment until a split,
	// so ownership must be shared across multiple directory entries.
	std::vector<std::shared_ptr<Segment>> segment_directory;
	uint8_t global_depth;
	uint64_t max_segment_size;
	PreModifyCallback pre_modify_cb_;

public:
	uint8_t GetGlobalDepth() const {
		return global_depth;
	}
	uint8_t GetSegmentLocalDepth(uint32_t dir_idx) const {
		return segment_directory[dir_idx]->local_depth;
	}
	uint32_t GetSegmentId(uint32_t dir_idx) const {
		return segment_directory[dir_idx]->segment_id;
	}
	bool IsDirectoryConsistent() const;
};
