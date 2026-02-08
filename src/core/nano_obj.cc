#include "core/nano_obj.h"
#include "core/util.h"
#include "core/unordered_dense.h"
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

namespace {
using SetType = ankerl::unordered_dense::set<std::string, ankerl::unordered_dense::hash<std::string>>;
using HashType = ankerl::unordered_dense::map<std::string, std::string, ankerl::unordered_dense::hash<std::string>>;
} // namespace

// --- Private helpers for copy/move/init ---

void NanoObj::CopyFrom(const NanoObj& other) {
	taglen = other.taglen;
	flag = other.flag;
	uint8_t tag = other.taglen;
	if (tag <= kInlineLen) {
		std::memcpy(u.data, other.u.data, tag);
	} else if (tag == INT_TAG) {
		u.ival = other.u.ival;
	} else if (tag == SMALL_STR_TAG) {
		u.small_str.length = other.u.small_str.length;
		std::memcpy(u.small_str.prefix, other.u.small_str.prefix, 4);
		u.small_str.ptr = static_cast<char*>(::operator new(other.u.small_str.length));
		std::memcpy(u.small_str.ptr, other.u.small_str.ptr, other.u.small_str.length);
	} else {
		u.ival = 0;
	}
}

void NanoObj::MoveFrom(NanoObj& other) noexcept {
	taglen = other.taglen;
	flag = other.flag;
	u = other.u;
	other.taglen = NULL_TAG;
	other.flag = 0;
}

void NanoObj::ResetToNull() {
	taglen = NULL_TAG;
	flag = 0;
}

void NanoObj::InitRobj(uint8_t type, uint8_t encoding) {
	Clear();
	u.robj.type = type;
	u.robj.encoding = encoding;
	u.robj.inner_obj = nullptr;
	u.robj.sz = 0;
	taglen = ROBJ_TAG;
	flag = 0;
}

// --- Constructors & Destructor ---

NanoObj::NanoObj() {
	ResetToNull();
}

NanoObj::NanoObj(std::string_view str) {
	ResetToNull();
	SetString(str);
}

NanoObj::NanoObj(int64_t val) {
	ResetToNull();
	SetInt(val);
}

NanoObj::NanoObj(const NanoObj& other) {
	CopyFrom(other);
}

NanoObj::NanoObj(NanoObj&& other) noexcept {
	MoveFrom(other);
}

NanoObj::~NanoObj() {
	Clear();
}

NanoObj& NanoObj::operator=(const NanoObj& other) {
	if (this != &other) {
		Clear();
		CopyFrom(other);
	}
	return *this;
}

NanoObj& NanoObj::operator=(NanoObj&& other) noexcept {
	if (this != &other) {
		Clear();
		MoveFrom(other);
	}
	return *this;
}

// --- Static Factory Methods ---

NanoObj NanoObj::FromString(std::string_view str) {
	return NanoObj(str);
}
NanoObj NanoObj::FromInt(int64_t val) {
	return NanoObj(val);
}

NanoObj NanoObj::FromKey(std::string_view key) {
	NanoObj obj;
	obj.SetStringKey(key);
	return obj;
}

NanoObj NanoObj::FromHash() {
	NanoObj obj;
	obj.SetHash();
	return obj;
}
NanoObj NanoObj::FromSet() {
	NanoObj obj;
	obj.SetSet();
	return obj;
}
NanoObj NanoObj::FromList() {
	NanoObj obj;
	obj.SetList();
	return obj;
}
NanoObj NanoObj::FromZset() {
	NanoObj obj;
	obj.SetZset();
	return obj;
}

// --- Type Query Methods ---

uint8_t NanoObj::GetTag() const {
	return taglen;
}
uint8_t NanoObj::GetFlag() const {
	return flag;
}
void NanoObj::SetTag(uint8_t tag) {
	taglen = tag;
}
void NanoObj::SetFlag(uint8_t flag_value) {
	flag = flag_value;
}

bool NanoObj::IsNull() const {
	return taglen == NULL_TAG;
}
bool NanoObj::IsInt() const {
	return taglen == INT_TAG;
}
bool NanoObj::IsString() const {
	return taglen <= kInlineLen || taglen == SMALL_STR_TAG;
}
bool NanoObj::IsHash() const {
	return taglen == ROBJ_TAG && u.robj.type == OBJ_HASH;
}
bool NanoObj::IsSet() const {
	return taglen == ROBJ_TAG && u.robj.type == OBJ_SET;
}
bool NanoObj::IsList() const {
	return taglen == ROBJ_TAG && u.robj.type == OBJ_LIST;
}
bool NanoObj::IsZset() const {
	return taglen == ROBJ_TAG && u.robj.type == OBJ_ZSET;
}

// --- Value Conversion Methods ---

std::optional<std::string_view> NanoObj::TryToString() const {
	if (taglen <= kInlineLen) {
		return std::string_view(reinterpret_cast<const char*>(u.data), taglen);
	}
	return std::nullopt;
}

std::optional<int64_t> NanoObj::TryToInt() const {
	return taglen == INT_TAG ? std::optional<int64_t>(u.ival) : std::nullopt;
}

std::string NanoObj::ToString() const {
	if (taglen <= kInlineLen) {
		return std::string(reinterpret_cast<const char*>(u.data), taglen);
	}
	if (taglen == INT_TAG) {
		return std::to_string(u.ival);
	}
	if (taglen == SMALL_STR_TAG) {
		return std::string(u.small_str.ptr, u.small_str.length);
	}
	return "";
}

int64_t NanoObj::AsInt() const {
	return taglen == INT_TAG ? u.ival : 0;
}

std::string_view NanoObj::GetStringView() const {
	if (taglen <= kInlineLen) {
		return std::string_view(reinterpret_cast<const char*>(u.data), taglen);
	}
	if (taglen == SMALL_STR_TAG) {
		return std::string_view(u.small_str.ptr, u.small_str.length);
	}
	return {};
}

int64_t NanoObj::GetIntValue() const {
	return AsInt();
}

// --- Property Access Methods ---

uint8_t NanoObj::GetType() const {
	if (taglen == ROBJ_TAG) {
		return u.robj.type;
	}
	return OBJ_STRING;
}

uint8_t NanoObj::GetEncoding() const {
	switch (taglen) {
	case NULL_TAG:
		return OBJ_ENCODING_RAW;
	case INT_TAG:
		return OBJ_ENCODING_INT;
	case SMALL_STR_TAG:
		return OBJ_ENCODING_RAW;
	case ROBJ_TAG:
		return u.robj.encoding;
	default:
		return taglen <= kInlineLen ? OBJ_ENCODING_EMBSTR : OBJ_ENCODING_RAW;
	}
}

size_t NanoObj::Size() const {
	if (taglen <= kInlineLen) {
		return taglen;
	}
	if (taglen == SMALL_STR_TAG) {
		return u.small_str.length;
	}
	if (taglen == INT_TAG) {
		return std::to_string(u.ival).length();
	}
	if (taglen == ROBJ_TAG) {
		return u.robj.sz;
	}
	return 0;
}

// --- Internal Setters ---

void NanoObj::Clear() {
	if (taglen == SMALL_STR_TAG) {
		FreeSmallString();
	} else if (taglen == ROBJ_TAG) {
		FreeRobj();
	}
}

void NanoObj::FreeRobj() {
	if (taglen != ROBJ_TAG || u.robj.inner_obj == nullptr) {
		return;
	}
	// Must use `delete` so destructors run (not `::operator delete`).
	switch (u.robj.type) {
	case OBJ_LIST:
		delete static_cast<std::deque<NanoObj>*>(u.robj.inner_obj);
		break;
	case OBJ_SET:
		delete static_cast<SetType*>(u.robj.inner_obj);
		break;
	case OBJ_HASH:
		delete static_cast<HashType*>(u.robj.inner_obj);
		break;
	default:
		::operator delete(u.robj.inner_obj);
		break;
	}
	u.robj.inner_obj = nullptr;
}

void NanoObj::FreeSmallString() {
	if (u.small_str.ptr != nullptr) {
		::operator delete(u.small_str.ptr);
		u.small_str.ptr = nullptr;
	}
}

void NanoObj::SetString(std::string_view str) {
	str.size() <= kInlineLen ? SetInlineString(str) : SetSmallString(str);
}

void NanoObj::SetInt(int64_t val) {
	Clear();
	u.ival = val;
	taglen = INT_TAG;
	flag = 0;
}

void NanoObj::SetInlineString(std::string_view str) {
	Clear();
	std::memcpy(u.data, str.data(), str.size());
	taglen = static_cast<uint8_t>(str.size());
	flag = 0;
}

void NanoObj::SetSmallString(std::string_view str) {
	Clear();
	u.small_str.length = static_cast<uint16_t>(str.size());
	taglen = SMALL_STR_TAG;
	flag = 0;
	size_t prefix_len = std::min(str.size(), size_t {4});
	std::memcpy(u.small_str.prefix, str.data(), prefix_len);
	u.small_str.ptr = static_cast<char*>(::operator new(str.size()));
	std::memcpy(u.small_str.ptr, str.data(), str.size());
}

void NanoObj::SetStringKey(std::string_view str) {
	if (str.size() <= 20) {
		int64_t ival;
		if (String2ll(str.data(), str.size(), &ival)) {
			SetInt(ival);
			return;
		}
	}
	SetString(str);
}

char* NanoObj::PrepareStringBuffer(size_t len) {
	Clear();
	flag = 0;
	if (len <= kInlineLen) {
		taglen = static_cast<uint8_t>(len);
		return u.data;
	}
	u.small_str.length = static_cast<uint16_t>(len);
	std::memset(u.small_str.prefix, 0, sizeof(u.small_str.prefix));
	u.small_str.ptr = static_cast<char*>(::operator new(len));
	taglen = SMALL_STR_TAG;
	return u.small_str.ptr;
}

void NanoObj::FinalizePreparedString() {
	if (taglen != SMALL_STR_TAG) {
		return;
	}
	size_t prefix_len = std::min(static_cast<size_t>(u.small_str.length), sizeof(u.small_str.prefix));
	if (prefix_len > 0) {
		std::memcpy(u.small_str.prefix, u.small_str.ptr, prefix_len);
	}
}

bool NanoObj::MaybeConvertToInt() {
	std::string_view sv = GetRawStringView();
	if (sv.empty() || sv.size() > 20) {
		return false;
	}
	int64_t ival;
	if (!String2ll(sv.data(), sv.size(), &ival)) {
		return false;
	}
	SetInt(ival);
	return true;
}

std::string_view NanoObj::GetRawStringView() const {
	return GetStringView();
}

// --- Type Setters ---

void NanoObj::SetHash() {
	InitRobj(OBJ_HASH, OBJ_ENCODING_HASHTABLE);
}
void NanoObj::SetSet() {
	InitRobj(OBJ_SET, OBJ_ENCODING_HASHTABLE);
}
void NanoObj::SetList() {
	InitRobj(OBJ_LIST, OBJ_ENCODING_RAW);
}
void NanoObj::SetZset() {
	InitRobj(OBJ_ZSET, OBJ_ENCODING_SKIPLIST);
}

// --- Comparison Operators ---

bool NanoObj::operator==(const NanoObj& other) const {
	if (taglen == INT_TAG && other.taglen == INT_TAG) {
		return u.ival == other.u.ival;
	}
	bool this_str = (taglen <= kInlineLen || taglen == SMALL_STR_TAG);
	bool other_str = (other.taglen <= kInlineLen || other.taglen == SMALL_STR_TAG);
	if (this_str && other_str) {
		return GetStringView() == other.GetStringView();
	}
	return false;
}

bool NanoObj::operator!=(const NanoObj& other) const {
	return !(*this == other);
}
