#include "storage/dash.h"
#include <cassert>
#include <algorithm>

template <typename K, typename V>
DashTable<K, V>::DashTable(uint64_t initial_segment_count, uint64_t max_segment_size)
    : global_depth_(0), max_segment_size_(max_segment_size) {
	assert(initial_segment_count > 0);
	assert((initial_segment_count & (initial_segment_count - 1)) == 0);

	global_depth_ = static_cast<uint8_t>(__builtin_ctz(initial_segment_count));
	segment_directory_.resize(initial_segment_count);

	for (uint32_t i = 0; i < initial_segment_count; ++i) {
		segment_directory_[i] = std::make_shared<Segment>(global_depth_, i, max_segment_size_);
	}
}

template <typename K, typename V>
DashTable<K, V>::DashTable(DashTable&& other) noexcept
    : segment_directory_(std::move(other.segment_directory_)), global_depth_(other.global_depth_),
      max_segment_size_(other.max_segment_size_) {
	other.global_depth_ = 0;
}

template <typename K, typename V>
DashTable<K, V>& DashTable<K, V>::operator=(DashTable&& other) noexcept {
	if (this != &other) {
		segment_directory_ = std::move(other.segment_directory_);
		global_depth_ = other.global_depth_;
		max_segment_size_ = other.max_segment_size_;

		other.global_depth_ = 0;
	}
	return *this;
}

template <typename K, typename V>
void DashTable<K, V>::Insert(const K& key, const V& value) {
	uint64_t seg_idx = GetSegmentIndex(key);
	auto& segment = segment_directory_[seg_idx];

	size_t old_size = segment->table.size();
	segment->table.insert_or_assign(key, value);
	size_t new_size = segment->table.size();

	if (NeedSplit(seg_idx)) {
		SplitSegment(seg_idx);
		if (new_size > old_size) {
			Insert(key, value);
		}
	}
}

template <typename K, typename V>
const V* DashTable<K, V>::Find(const K& key) const {
	uint64_t seg_idx = GetSegmentIndex(key);
	const auto& segment = segment_directory_[seg_idx];

	auto it = segment->table.find(key);
	if (it != segment->table.end()) {
		return &(it->second);
	}
	return nullptr;
}

template <typename K, typename V>
bool DashTable<K, V>::Erase(const K& key) {
	uint64_t seg_idx = GetSegmentIndex(key);
	auto& segment = segment_directory_[seg_idx];

	return segment->table.erase(key) > 0;
}

template <typename K, typename V>
void DashTable<K, V>::Clear() {
	for (auto& segment : segment_directory_) {
		segment->table.clear();
	}
}

template <typename K, typename V>
uint64_t DashTable<K, V>::Size() const {
	uint64_t size = 0;
	for (size_t i = 0; i < segment_directory_.size(); i = NextSeg(i)) {
		size += segment_directory_[i]->table.size();
	}
	return size;
}

template <typename K, typename V>
uint64_t DashTable<K, V>::SegmentCount() const {
	return segment_directory_.size();
}

template <typename K, typename V>
uint64_t DashTable<K, V>::BucketCount() const {
	uint64_t count = 0;
	for (size_t i = 0; i < segment_directory_.size(); i = NextSeg(i)) {
		count += segment_directory_[i]->table.bucket_count();
	}
	return count;
}

template <typename K, typename V>
uint64_t DashTable<K, V>::GetSegmentIndex(const K& key) const {
	uint64_t hash = std::hash<K> {}(key);
	if (global_depth_ == 0) {
		return 0;
	}
	return hash >> (64 - global_depth_);
}

template <typename K, typename V>
bool DashTable<K, V>::NeedSplit(uint32_t seg_id) const {
	const auto& segment = segment_directory_[seg_id];
	return segment->table.size() >= max_segment_size_;
}

template <typename K, typename V>
void DashTable<K, V>::SplitSegment(uint32_t seg_id) {
	Segment* source = segment_directory_[seg_id].get();
	assert(source->local_depth > 0);

	if (source->local_depth == global_depth_) {
		uint64_t old_size = segment_directory_.size();
		segment_directory_.resize(old_size * 2);

		for (uint64_t i = 0; i < old_size; ++i) {
			segment_directory_[old_size + i] = segment_directory_[i];
		}

		global_depth_++;
	}

	uint32_t chunk_size = 1ULL << (global_depth_ - source->local_depth);
	uint32_t new_id = seg_id + chunk_size / 2;

	auto new_segment = std::make_shared<Segment>(source->local_depth + 1, new_id, source->table.size() / 2 + 64);

	uint64_t mask = (1ULL << global_depth_) - 1;
	for (auto it = source->table.begin(); it != source->table.end();) {
		uint64_t hash = std::hash<K> {}(it->first);
		uint32_t new_idx = hash & mask;

		if (new_idx != seg_id) {
			new_segment->table.insert(std::move(*it));
			it = source->table.erase(it);
		} else {
			++it;
		}
	}

	source->local_depth++;

	for (uint64_t i = new_id; i < new_id + chunk_size; ++i) {
		if (i < segment_directory_.size()) {
			segment_directory_[i] = new_segment;
		}
	}
}

template <typename K, typename V>
size_t DashTable<K, V>::NextSeg(size_t sid) const {
	if (sid >= segment_directory_.size()) {
		return sid;
	}
	size_t delta = (1ULL << (global_depth_ - segment_directory_[sid]->local_depth));
	return sid + delta;
}

template class DashTable<std::string, std::string>;
