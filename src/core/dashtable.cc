#include "core/dashtable.h"
#include "core/nano_obj.h"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <vector>

template <typename K, typename V>
DashTable<K, V>::DashTable(uint64_t initial_segment_count, uint64_t max_segment_size)
    : global_depth_(0), max_segment_size_(max_segment_size) {
	assert(initial_segment_count > 0);
	assert((initial_segment_count & (initial_segment_count - 1)) == 0);

	global_depth_ = static_cast<uint8_t>(__builtin_ctz(initial_segment_count));
	segment_directory_.reserve(initial_segment_count);

	for (uint32_t i = 0; i < initial_segment_count; ++i) {
		segment_directory_.push_back(std::make_shared<Segment>(global_depth_, i, max_segment_size_));
	}
}

template <typename K, typename V>
DashTable<K, V>::~DashTable() {
}

template <typename K, typename V>
DashTable<K, V>::DashTable(DashTable&& other) noexcept
    : segment_directory_(std::move(other.segment_directory_)),
      global_depth_(other.global_depth_),
      max_segment_size_(other.max_segment_size_) {
	other.global_depth_ = 0;
	other.segment_directory_.clear();
	other.segment_directory_.resize(1);
	other.segment_directory_[0] = std::make_shared<Segment>(other.global_depth_, 0, other.max_segment_size_);
}

template <typename K, typename V>
DashTable<K, V>& DashTable<K, V>::operator=(DashTable&& other) noexcept {
	if (this != &other) {
		segment_directory_ = std::move(other.segment_directory_);
		global_depth_ = other.global_depth_;
		max_segment_size_ = other.max_segment_size_;

		other.global_depth_ = 0;
		other.segment_directory_.clear();
		other.segment_directory_.resize(1);
		other.segment_directory_[0] = std::make_shared<Segment>(other.global_depth_, 0, other.max_segment_size_);
	}
	return *this;
}

template <typename K, typename V>
void DashTable<K, V>::Insert(const K& key, V&& value) {
	uint64_t seg_idx = GetSegmentIndex(key);
	Segment* segment = segment_directory_[seg_idx].get();

	segment->table.insert_or_assign(key, std::forward<V>(value));

	while (NeedSplit(seg_idx)) {
		SplitSegment(seg_idx);
		seg_idx = GetSegmentIndex(key);
		segment = segment_directory_[seg_idx].get();
	}
}

template <typename K, typename V>
void DashTable<K, V>::Insert(const K& key, const V& value) {
	Insert(key, V(value));
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
	uint64_t hash = ankerl::unordered_dense::hash<K> {}(key);
	if (global_depth_ == 0) {
		return 0;
	}
	return hash >> (64 - global_depth_);
}

template <typename K, typename V>
bool DashTable<K, V>::NeedSplit(uint32_t seg_id) const {
	const auto& segment = segment_directory_[seg_id];
	return segment->table.size() >= max_segment_size_ * kSplitThreshold;
}

template <typename K, typename V>
void DashTable<K, V>::SplitSegment(uint32_t seg_id) {
  Segment* source = segment_directory_[seg_id].get();

  if (source->local_depth == global_depth_) {
    uint64_t old_size = segment_directory_.size();
    segment_directory_.resize(old_size * 2);

    uint64_t repl_cnt = 1ULL << 1;
    for (int64_t i = old_size - 1; i >= 0; --i) {
      uint64_t offs = i * repl_cnt;
      for (uint64_t j = 0; j < repl_cnt; ++j) {
        segment_directory_[offs + j] = segment_directory_[i];
      }
      segment_directory_[i]->segment_id = offs;
    }

    global_depth_++;

    seg_id = source->segment_id;
    source = segment_directory_[seg_id].get();
  }

  uint32_t chunk_size = 1ULL << (global_depth_ - source->local_depth);
  uint32_t start_idx = seg_id & (~(chunk_size - 1));
  uint32_t chunk_mid = start_idx + chunk_size / 2;

  auto new_segment = std::make_shared<Segment>(source->local_depth + 1, chunk_mid, source->table.size() / 2);

  auto source_shared_ptr = segment_directory_[seg_id];

  source->segment_id = start_idx;
  source->local_depth++;

  std::vector<std::pair<uint64_t, K>> items;
  items.reserve(source->table.size());

  for (auto it = source->table.begin(); it != source->table.end(); ++it) {
    uint64_t hash = ankerl::unordered_dense::hash<K>{}(it->first);
    items.push_back({hash, it->first});
  }

  for (const auto& [hash, key] : items) {
    uint32_t new_idx = hash >> (64 - global_depth_);

    if (new_idx >= chunk_mid && new_idx < start_idx + chunk_size) {
      auto it = source->table.find(key);
      if (it != source->table.end()) {
        new_segment->table.insert(std::move(*it));
        source->table.erase(it);
      }
    }
  }

  for (uint64_t i = chunk_mid; i < start_idx + chunk_size; ++i) {
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

template <typename K, typename V>
bool DashTable<K, V>::IsDirectoryConsistent() const {
	if (global_depth_ > 64) {
		return false;
	}

	uint64_t expected_dir_size = global_depth_ == 0 ?1 : (1ULL << global_depth_);
	if (segment_directory_.size() != expected_dir_size) {
		return false;
	}

	for (size_t i = 0; i < segment_directory_.size(); ++i) {
		if (segment_directory_[i]->local_depth > global_depth_) {
			return false;
		}
	}

	for (size_t i = 0; i < segment_directory_.size();) {
		Segment* seg = segment_directory_[i].get();
		uint32_t chunk_size = 1ULL << (global_depth_ - seg->local_depth);
		uint32_t start_idx = i & (~(chunk_size - 1));

		for (size_t j = 1; j < chunk_size; ++j) {
			if (segment_directory_[start_idx + j].get() != seg) {
				return false;
			}
		}

		if (seg->segment_id != start_idx) {
			return false;
		}

		i += chunk_size;
	}

	return true;
}

template class DashTable<std::string, std::string>;
template class DashTable<std::string, NanoObj>;
template class DashTable<int, int>;
template class DashTable<int, double>;
template class DashTable<NanoObj, NanoObj>;
