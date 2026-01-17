#include "core/nano_obj.h"
#include "core/util.h"
#include <cstdlib>
#include <cstring>
#include <string>

// ============================================================================
// Tag & Flag Accessors
// ============================================================================

uint8_t NanoObj::getTag() const {
    return taglen_;
}

uint8_t NanoObj::getFlag() const {
    return flag_;
}

void NanoObj::setTag(uint8_t tag) {
    taglen_ = tag;
}

void NanoObj::setFlag(uint8_t flag) {
    flag_ = flag;
}

// ============================================================================
// Constructors & Destructor
// ============================================================================

NanoObj::NanoObj() {
    setTag(NULL_TAG);
    setFlag(0);
}

NanoObj::NanoObj(std::string_view str) {
    setTag(NULL_TAG);
    setFlag(0);
    setString(str);
}

NanoObj::NanoObj(int64_t val) {
    setTag(NULL_TAG);
    setFlag(0);
    setInt(val);
}

NanoObj::NanoObj(const NanoObj& other) {
    taglen_ = other.taglen_;
    flag_ = other.flag_;

    uint8_t tag = other.getTag();
    if (tag <= kInlineLen) {
        std::memcpy(u_.data, other.u_.data, tag);
    } else if (tag == INT_TAG) {
        u_.ival = other.u_.ival;
    } else if (tag == SMALL_STR_TAG) {
        u_.small_str.length = other.u_.small_str.length;
        std::memcpy(u_.small_str.prefix, other.u_.small_str.prefix, 4);
        u_.small_str.ptr = static_cast<char*>(::operator new(other.u_.small_str.length));
        std::memcpy(u_.small_str.ptr, other.u_.small_str.ptr, other.u_.small_str.length);
    } else {
        u_.ival = 0;
    }
}

NanoObj::NanoObj(NanoObj&& other) noexcept {
    taglen_ = other.taglen_;
    flag_ = other.flag_;
    u_ = other.u_;

    other.setTag(NULL_TAG);
    other.setFlag(0);
}

NanoObj::~NanoObj() {
    clear();
}

// ============================================================================
// Move Assignment Operator
// ============================================================================

NanoObj& NanoObj::operator=(NanoObj&& other) noexcept {
    if (this != &other) {
        clear();

        taglen_ = other.taglen_;
        flag_ = other.flag_;
        u_ = other.u_;

        other.setTag(NULL_TAG);
        other.setFlag(0);
    }
    return *this;
}

NanoObj& NanoObj::operator=(const NanoObj& other) {
    if (this != &other) {
        clear();

        taglen_ = other.taglen_;
        flag_ = other.flag_;

        uint8_t tag = other.getTag();
        if (tag <= kInlineLen) {
            std::memcpy(u_.data, other.u_.data, tag);
        } else if (tag == INT_TAG) {
            u_.ival = other.u_.ival;
        } else if (tag == SMALL_STR_TAG) {
            u_.small_str.length = other.u_.small_str.length;
            std::memcpy(u_.small_str.prefix, other.u_.small_str.prefix, 4);
            u_.small_str.ptr = static_cast<char*>(::operator new(other.u_.small_str.length));
            std::memcpy(u_.small_str.ptr, other.u_.small_str.ptr, other.u_.small_str.length);
        } else {
            u_.ival = 0;
        }
    }
    return *this;
}

// ============================================================================
// Static Factory Methods
// ============================================================================

NanoObj NanoObj::fromString(std::string_view str) {
    return NanoObj(str);
}

NanoObj NanoObj::fromInt(int64_t val) {
    return NanoObj(val);
}

// ============================================================================
// Type Query Methods
// ============================================================================

bool NanoObj::isNull() const {
    return getTag() == NULL_TAG;
}

bool NanoObj::isInt() const {
    return getTag() == INT_TAG;
}

bool NanoObj::isString() const {
    return (getTag() <= kInlineLen) || getTag() == SMALL_STR_TAG;
}

bool NanoObj::isHash() const {
    return u_.robj.type_ == OBJ_HASH;
}

bool NanoObj::isSet() const {
    return u_.robj.type_ == OBJ_SET;
}

bool NanoObj::isList() const {
    return u_.robj.type_ == OBJ_LIST;
}

bool NanoObj::isZset() const {
    return u_.robj.type_ == OBJ_ZSET;
}

// ============================================================================
// Value Conversion Methods
// ============================================================================

std::optional<std::string_view> NanoObj::tryToString() const {
    if (getTag() <= kInlineLen) {
        return std::string_view(reinterpret_cast<const char*>(u_.data), getTag());
    }
    return std::nullopt;
}

std::optional<int64_t> NanoObj::tryToInt() const {
    if (getTag() == INT_TAG) {
        return u_.ival;
    }
    return std::nullopt;
}

std::string NanoObj::toString() const {
    auto sv = tryToString();
    if (sv) {
        return std::string(*sv);
    }
    if (getTag() == INT_TAG) {
        return std::to_string(u_.ival);
    }
    if (getTag() == SMALL_STR_TAG) {
        return std::string(u_.small_str.ptr, u_.small_str.length);
    }
    return "";
}

int64_t NanoObj::asInt() const {
    if (getTag() == INT_TAG) {
        return u_.ival;
    }
    return 0;
}

// ============================================================================
// Property Access Methods
// ============================================================================

uint8_t NanoObj::getType() const {
    if (getTag() == INT_TAG || getTag() <= kInlineLen || getTag() == SMALL_STR_TAG) {
        return OBJ_STRING;
    }
    if (getTag() == ROBJ_TAG) {
        return u_.robj.type_;
    }
    return OBJ_STRING;
}

uint8_t NanoObj::getEncoding() const {
    if (getTag() == NULL_TAG) {
        return OBJ_ENCODING_RAW;
    }
    if (getTag() == INT_TAG) {
        return OBJ_ENCODING_INT;
    }
    if (getTag() <= kInlineLen) {
        return OBJ_ENCODING_EMBSTR;
    }
    if (getTag() == SMALL_STR_TAG) {
        return OBJ_ENCODING_RAW;
    }
    if (getTag() == ROBJ_TAG) {
        return u_.robj.encoding_;
    }
    return OBJ_ENCODING_RAW;
}

size_t NanoObj::size() const {
    if (getTag() <= kInlineLen) {
        return getTag();
    }
    if (getTag() == SMALL_STR_TAG) {
        return u_.small_str.length;
    }
    if (getTag() == INT_TAG) {
        return std::to_string(u_.ival).length();
    }
    if (getTag() == ROBJ_TAG) {
        return u_.robj.sz_;
    }
    return 0;
}

// ============================================================================
// Internal Setters
// ============================================================================

void NanoObj::clear() {
    if (getTag() == SMALL_STR_TAG) {
        freeSmallString();
    } else if (getTag() == ROBJ_TAG) {
        if (u_.robj.inner_obj_ != nullptr) {
            ::operator delete(u_.robj.inner_obj_);
        }
    }
}

void NanoObj::setString(std::string_view str) {
    if (str.size() <= kInlineLen) {
        setInlineString(str);
    } else {
        setSmallString(str);
    }
}

void NanoObj::setInt(int64_t val) {
    clear();
    u_.ival = val;
    setTag(INT_TAG);
    setFlag(0);
}

void NanoObj::setInlineString(std::string_view str) {
    clear();
    std::memcpy(u_.data, str.data(), str.size());
    setTag(static_cast<uint8_t>(str.size()));
    setFlag(0);
}

void NanoObj::setSmallString(std::string_view str) {
    clear();
    u_.small_str.length = static_cast<uint16_t>(str.size());
    setTag(SMALL_STR_TAG);
    setFlag(0);

    size_t prefix_len = std::min(str.size(), size_t{4});
    std::memcpy(u_.small_str.prefix, str.data(), prefix_len);

    u_.small_str.ptr = static_cast<char*>(::operator new(str.size()));
    std::memcpy(u_.small_str.ptr, str.data(), str.size());
}

void NanoObj::freeSmallString() {
    if (u_.small_str.ptr != nullptr) {
        ::operator delete(u_.small_str.ptr);
        u_.small_str.ptr = nullptr;
    }
}

// ============================================================================
// Key-specific Methods
// ============================================================================

NanoObj NanoObj::fromKey(std::string_view key) {
    NanoObj obj;
    obj.setStringKey(key);
    return obj;
}

void NanoObj::setStringKey(std::string_view str) {
    if (str.size() <= 20) {
        int64_t ival;
        if (string2ll(str.data(), str.size(), &ival)) {
            setInt(ival);
            return;
        }
    }
    setString(str);
}

std::string_view NanoObj::getRawStringView() const {
    if (getTag() <= kInlineLen) {
        return std::string_view(reinterpret_cast<const char*>(u_.data), getTag());
    }
    if (getTag() == SMALL_STR_TAG) {
        return std::string_view(u_.small_str.ptr, u_.small_str.length);
    }
    return std::string_view();
}

bool NanoObj::operator==(const NanoObj& other) const {
    uint8_t this_tag = getTag();
    uint8_t other_tag = other.getTag();

    if (this_tag == INT_TAG && other_tag == INT_TAG) {
        return u_.ival == other.u_.ival;
    }

    if ((this_tag <= kInlineLen || this_tag == SMALL_STR_TAG) &&
        (other_tag <= kInlineLen || other_tag == SMALL_STR_TAG)) {
        std::string_view this_str = getRawStringView();
        std::string_view other_str = other.getRawStringView();
        return this_str == other_str;
    }

    if (this_tag == INT_TAG && (other_tag <= kInlineLen || other_tag == SMALL_STR_TAG)) {
        std::string other_str_val = other.toString();
        std::string this_str_val = toString();
        return this_str_val == other_str_val;
    }

    if (other_tag == INT_TAG && (this_tag <= kInlineLen || this_tag == SMALL_STR_TAG)) {
        std::string this_str_val = toString();
        std::string other_str_val = other.toString();
        return this_str_val == other_str_val;
    }

    return false;
}

bool NanoObj::operator!=(const NanoObj& other) const {
    return !(*this == other);
}

// ============================================================================
// Comparison Helpers
// ============================================================================

std::string_view NanoObj::getStringView() const {
    if (getTag() <= kInlineLen) {
        return std::string_view(reinterpret_cast<const char*>(u_.data), getTag());
    }
    if (getTag() == SMALL_STR_TAG) {
        return std::string_view(u_.small_str.ptr, u_.small_str.length);
    }
    return std::string_view();
}

int64_t NanoObj::getIntValue() const {
    if (getTag() == INT_TAG) {
        return u_.ival;
    }
    return 0;
}

// ============================================================================
// Factory Methods for Different Types
// ============================================================================

NanoObj NanoObj::fromHash() {
    NanoObj obj;
    obj.setHash();
    return obj;
}

NanoObj NanoObj::fromSet() {
    NanoObj obj;
    obj.setSet();
    return obj;
}

NanoObj NanoObj::fromList() {
    NanoObj obj;
    obj.setList();
    return obj;
}

NanoObj NanoObj::fromZset() {
    NanoObj obj;
    obj.setZset();
    return obj;
}

// ============================================================================
// Type Setters for Different Types
// ============================================================================

void NanoObj::setHash() {
    clear();
    u_.robj.type_ = OBJ_HASH;
    u_.robj.encoding_ = OBJ_ENCODING_HASHTABLE;
    u_.robj.inner_obj_ = nullptr;
    u_.robj.sz_ = 0;
    setTag(ROBJ_TAG);
    setFlag(0);
}

void NanoObj::setSet() {
    clear();
    u_.robj.type_ = OBJ_SET;
    u_.robj.encoding_ = OBJ_ENCODING_HASHTABLE;
    u_.robj.inner_obj_ = nullptr;
    u_.robj.sz_ = 0;
    setTag(ROBJ_TAG);
    setFlag(0);
}

void NanoObj::setList() {
    clear();
    u_.robj.type_ = OBJ_LIST;
    u_.robj.encoding_ = OBJ_ENCODING_RAW;
    u_.robj.inner_obj_ = nullptr;
    u_.robj.sz_ = 0;
    setTag(ROBJ_TAG);
    setFlag(0);
}

void NanoObj::setZset() {
    clear();
    u_.robj.type_ = OBJ_ZSET;
    u_.robj.encoding_ = OBJ_ENCODING_SKIPLIST;
    u_.robj.inner_obj_ = nullptr;
    u_.robj.sz_ = 0;
    setTag(ROBJ_TAG);
    setFlag(0);
}
// ============================================================================
// Factory Methods for Different Types
// ============================================================================

// ============================================================================
// Template Methods
// ============================================================================

// Remove template method implementations from .cc file

