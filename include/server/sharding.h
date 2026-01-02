#pragma once

#include <cstddef>
#include <string_view>
#include <functional>
#include <cstdint>

inline size_t Shard(std::string_view key, size_t num_shards) {
	if (num_shards <= 1) return 0;

	uint64_t hash = std::hash<std::string_view>{}(key);
	return hash % num_shards;
}
