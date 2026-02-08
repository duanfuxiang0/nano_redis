#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

bool String2ll(const char* s, size_t slen, int64_t* value);

// Case-insensitive comparison (ASCII only, no locale support)
bool EqualsIgnoreCase(std::string_view a, std::string_view b);
