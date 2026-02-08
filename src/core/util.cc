#include "core/util.h"
#include <cstdint>
#include <cstddef>
#include <climits>
#include <string_view>

bool String2ll(const char* s, size_t slen, int64_t* value) {
	const char* p = s;
	size_t plen = 0;
	bool negative = false;
	uint64_t v = 0;

	if (slen == 0 || slen >= 21) {
		return false;
	}

	if (slen == 1 && p[0] >= '0' && p[0] <= '9') {
		if (value != nullptr) {
			*value = p[0] - '0';
		}
		return true;
	}

	if (p[0] == '-') {
		negative = true;
		p++;
		plen++;

		if (plen == slen) {
			return false;
		}
	}

	if (p[0] >= '1' && p[0] <= '9') {
		v = static_cast<uint64_t>(p[0] - '0');
		p++;
		plen++;
	} else {
		return false;
	}

	while (plen < slen && p[0] >= '0' && p[0] <= '9') {
		if (v > (UINT64_MAX / 10)) {
			return false;
		}
		v *= 10;

		uint64_t digit = static_cast<uint64_t>(p[0] - '0');
		if (v > (UINT64_MAX - digit)) {
			return false;
		}
		v += digit;

		p++;
		plen++;
	}

	if (plen < slen) {
		return false;
	}

	if (negative) {
		uint64_t max_negative = static_cast<uint64_t>(-(INT64_MIN + 1)) + 1;
		if (v > max_negative) {
			return false;
		}
		if (value != nullptr) {
			if (v == max_negative) {
				*value = INT64_MIN;
			} else {
				*value = -static_cast<int64_t>(v);
			}
		}
	} else {
		if (v > static_cast<uint64_t>(INT64_MAX)) {
			return false;
		}
		if (value != nullptr) {
			*value = static_cast<int64_t>(v);
		}
	}
	return true;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		unsigned char ca = static_cast<unsigned char>(a[i]);
		unsigned char cb = static_cast<unsigned char>(b[i]);
		unsigned char ua = (ca >= 'a' && ca <= 'z') ? static_cast<unsigned char>(ca - 'a' + 'A') : ca;
		unsigned char ub = (cb >= 'a' && cb <= 'z') ? static_cast<unsigned char>(cb - 'a' + 'A') : cb;
		if (ua != ub) {
			return false;
		}
	}
	return true;
}
