#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "unordered_dense.h"

namespace {
constexpr uint64_t kInitialSegmentCount = 1;
constexpr uint64_t kDefaultFixedBucketCount = 16;
constexpr float kSplitThreshold = 0.8f;
}

template <typename K, typename V>
class DashTable {
public:
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

	template <typename Func>
	void ForEach(Func&& func) const {
		for (size_t i = 0; i < segment_directory_.size(); i = NextSeg(i)) {
			const auto& segment = segment_directory_[i];
			for (const auto& [key, value] : segment->table) {
				func(key, value);
			}
		}
	}

private:
	struct Segment {
		ankerl::unordered_dense::map<K, V, ankerl::unordered_dense::hash<K>> table;
		uint8_t local_depth;
		uint32_t segment_id;

		explicit Segment(uint8_t depth, uint32_t id, uint64_t fixed_bucket_count)
		    : table(fixed_bucket_count), local_depth(depth), segment_id(id) {
			table.max_load_factor(1.0f);
		}
	};

	uint64_t GetSegmentIndex(const K& key) const;
	void SplitSegment(uint32_t seg_id);
	bool NeedSplit(uint32_t seg_id) const;
	size_t NextSeg(size_t sid) const;

	std::vector<std::shared_ptr<Segment>> segment_directory_;
	uint8_t global_depth_;
	uint64_t max_segment_size_;

public:
	uint8_t GetGlobalDepth() const { return global_depth_; }
	uint8_t GetSegmentLocalDepth(uint32_t dir_idx) const { return segment_directory_[dir_idx]->local_depth; }
	uint32_t GetSegmentId(uint32_t dir_idx) const { return segment_directory_[dir_idx]->segment_id; }
	bool IsDirectoryConsistent() const;
};
