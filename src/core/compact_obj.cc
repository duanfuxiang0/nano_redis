#include "core/compact_obj.h"
#include "core/util.h"
#include <cstdlib>
#include <cstring>
#include <string>

// ============================================================================
// Tag & Flag Accessors
// ============================================================================

uint8_t CompactObj::getTag() const {
    return taglen_;
}

uint8_t CompactObj::getFlag() const {
    return flag_;
}

void CompactObj::setTag(uint8_t tag) {
    taglen_ = tag;
}

void CompactObj::setFlag(uint8_t flag) {
    flag_ = flag;
}

// ============================================================================
// Constructors & Destructor
// ============================================================================

CompactObj::CompactObj() {
    setTag(NULL_TAG);
    setFlag(0);
}

CompactObj::CompactObj(std::string_view str) {
    setTag(NULL_TAG);
    setFlag(0);
    setString(str);
}

CompactObj::CompactObj(int64_t val) {
    setTag(NULL_TAG);
    setFlag(0);
    setInt(val);
}

CompactObj::CompactObj(const CompactObj& other) {
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

CompactObj::CompactObj(CompactObj&& other) noexcept {
    taglen_ = other.taglen_;
    flag_ = other.flag_;
    u_ = other.u_;

    other.setTag(NULL_TAG);
    other.setFlag(0);
}

CompactObj::~CompactObj() {
    clear();
}

// ============================================================================
// Move Assignment Operator
// ============================================================================

CompactObj& CompactObj::operator=(CompactObj&& other) noexcept {
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

CompactObj& CompactObj::operator=(const CompactObj& other) {
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

CompactObj CompactObj::fromString(std::string_view str) {
    return CompactObj(str);
}

CompactObj CompactObj::fromInt(int64_t val) {
    return CompactObj(val);
}

// ============================================================================
// Type Query Methods
// ============================================================================

bool CompactObj::isNull() const {
    return getTag() == NULL_TAG;
}

bool CompactObj::isInt() const {
    return getTag() == INT_TAG;
}

bool CompactObj::isString() const {
    return (getTag() <= kInlineLen) || getTag() == SMALL_STR_TAG;
}

// ============================================================================
// Value Conversion Methods
// ============================================================================

std::optional<std::string_view> CompactObj::tryToString() const {
    if (getTag() <= kInlineLen) {
        return std::string_view(reinterpret_cast<const char*>(u_.data), getTag());
    }
    return std::nullopt;
}

std::optional<int64_t> CompactObj::tryToInt() const {
    if (getTag() == INT_TAG) {
        return u_.ival;
    }
    return std::nullopt;
}

std::string CompactObj::toString() const {
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

int64_t CompactObj::asInt() const {
    if (getTag() == INT_TAG) {
        return u_.ival;
    }
    return 0;
}

// ============================================================================
// Property Access Methods
// ============================================================================

uint8_t CompactObj::getType() const {
    if (getTag() == INT_TAG || getTag() <= kInlineLen || getTag() == SMALL_STR_TAG) {
        return OBJ_STRING;
    }
    if (getTag() == ROBJ_TAG) {
        return u_.robj.type_;
    }
    return OBJ_STRING;
}

uint8_t CompactObj::getEncoding() const {
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

size_t CompactObj::size() const {
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

void CompactObj::clear() {
    if (getTag() == SMALL_STR_TAG) {
        freeSmallString();
    } else if (getTag() == ROBJ_TAG) {
        if (u_.robj.inner_obj_ != nullptr) {
            ::operator delete(u_.robj.inner_obj_);
        }
    }
}

void CompactObj::setString(std::string_view str) {
    if (str.size() <= kInlineLen) {
        setInlineString(str);
    } else {
        setSmallString(str);
    }
}

void CompactObj::setInt(int64_t val) {
    clear();
    u_.ival = val;
    setTag(INT_TAG);
    setFlag(0);
}

void CompactObj::setInlineString(std::string_view str) {
    clear();
    std::memcpy(u_.data, str.data(), str.size());
    setTag(static_cast<uint8_t>(str.size()));
    setFlag(0);
}

void CompactObj::setSmallString(std::string_view str) {
    clear();
    u_.small_str.length = static_cast<uint16_t>(str.size());
    setTag(SMALL_STR_TAG);
    setFlag(0);

    size_t prefix_len = std::min(str.size(), size_t{4});
    std::memcpy(u_.small_str.prefix, str.data(), prefix_len);

    u_.small_str.ptr = static_cast<char*>(::operator new(str.size()));
    std::memcpy(u_.small_str.ptr, str.data(), str.size());
}

void CompactObj::freeSmallString() {
    if (u_.small_str.ptr != nullptr) {
        ::operator delete(u_.small_str.ptr);
        u_.small_str.ptr = nullptr;
    }
}

// ============================================================================
// Key-specific Methods
// ============================================================================

CompactObj CompactObj::fromKey(std::string_view key) {
    CompactObj obj;
    obj.setStringKey(key);
    return obj;
}

void CompactObj::setStringKey(std::string_view str) {
    if (str.size() <= 20) {
        int64_t ival;
        if (string2ll(str.data(), str.size(), &ival)) {
            setInt(ival);
            return;
        }
    }
    setString(str);
}

std::string_view CompactObj::getRawStringView() const {
    if (getTag() <= kInlineLen) {
        return std::string_view(reinterpret_cast<const char*>(u_.data), getTag());
    }
    if (getTag() == SMALL_STR_TAG) {
        return std::string_view(u_.small_str.ptr, u_.small_str.length);
    }
    return std::string_view();
}

bool CompactObj::operator==(const CompactObj& other) const {
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

bool CompactObj::operator!=(const CompactObj& other) const {
    return !(*this == other);
}

// ============================================================================
// Comparison Helpers
// ============================================================================

std::string_view CompactObj::getStringView() const {
    if (getTag() <= kInlineLen) {
        return std::string_view(reinterpret_cast<const char*>(u_.data), getTag());
    }
    if (getTag() == SMALL_STR_TAG) {
        return std::string_view(u_.small_str.ptr, u_.small_str.length);
    }
    return std::string_view();
}

int64_t CompactObj::getIntValue() const {
    if (getTag() == INT_TAG) {
        return u_.ival;
    }
    return 0;
}
