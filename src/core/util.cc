#include "core/util.h"
#include <cstdint>
#include <cstddef>
#include <climits>

bool string2ll(const char* s, size_t slen, int64_t* value) {
	const char* p = s;
	size_t plen = 0;
	int negative = 0;
	unsigned long long v;

	if (slen == 0 || slen >= 21) {
		return 0;
	}

	if (slen == 1 && p[0] >= '0' && p[0] <= '9') {
		if (value != nullptr) *value = p[0] - '0';
		return 1;
	}

	if (p[0] == '-') {
		negative = 1;
		p++;
		plen++;

		if (plen == slen) return 0;
	}

	if (p[0] >= '1' && p[0] <= '9') {
		v = p[0] - '0';
		p++;
		plen++;
	} else {
		return 0;
	}

	while (plen < slen && p[0] >= '0' && p[0] <= '9') {
		if (v > (ULLONG_MAX / 10)) return 0;
		v *= 10;

		if (v > (ULLONG_MAX - (p[0] - '0'))) return 0;
		v += p[0] - '0';

		p++;
		plen++;
	}

	if (plen < slen) return 0;

	if (negative) {
		if (v > ((unsigned long long)(-(LLONG_MIN + 1)) + 1)) return 0;
		if (value != nullptr) *value = -v;
	} else {
		if (v > LLONG_MAX) return 0;
		if (value != nullptr) *value = v;
	}
	return 1;
}
