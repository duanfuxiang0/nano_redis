#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <memory>

#include "unordered_dense.h"

namespace {
constexpr uint64_t kInitialSegmentCount = 1;
constexpr uint64_t kDefaultMaxSegmentSize = 1024;
} // namespace

template <typename K, typename V>
class DashTable {
public:
	explicit DashTable(uint64_t initial_segment_count = kInitialSegmentCount,
	                   uint64_t max_segment_size = kDefaultMaxSegmentSize);

	~DashTable() = default;

	DashTable(const DashTable&) = delete;
	DashTable& operator=(const DashTable&) = delete;

	DashTable(DashTable&& other) noexcept;
	DashTable& operator=(DashTable&& other) noexcept;

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
		ankerl::unordered_dense::map<K, V> table;
		uint8_t local_depth;
		uint32_t segment_id;

		explicit Segment(uint8_t depth, uint32_t id, uint64_t capacity_hint)
		    : table(capacity_hint), local_depth(depth), segment_id(id) {
		}
	};

	uint64_t GetSegmentIndex(const K& key) const;
	void SplitSegment(uint32_t seg_id);
	bool NeedSplit(uint32_t seg_id) const;
	size_t NextSeg(size_t sid) const;

	std::vector<std::shared_ptr<Segment>> segment_directory_;
	uint8_t global_depth_;
	uint64_t max_segment_size_;
};
