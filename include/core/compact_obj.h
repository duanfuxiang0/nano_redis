#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <optional>
#include <new>
#include <memory>
#include <deque>

constexpr size_t kInlineLen = 14;

constexpr uint8_t OBJ_STRING = 0;
constexpr uint8_t OBJ_HASH = 1;
constexpr uint8_t OBJ_SET = 2;
constexpr uint8_t OBJ_LIST = 3;
constexpr uint8_t OBJ_ZSET = 4;

constexpr uint8_t OBJ_ENCODING_RAW = 0;
constexpr uint8_t OBJ_ENCODING_INT = 1;
constexpr uint8_t OBJ_ENCODING_EMBSTR = 8;
constexpr uint8_t OBJ_ENCODING_HASHTABLE = 2;
constexpr uint8_t OBJ_ENCODING_SKIPLIST = 7;

class RobjWrapper {
public:
	void* inner_obj_;
	uint32_t sz_;
	uint8_t type_;
	uint8_t encoding_;
} __attribute__((packed));

static_assert(sizeof(RobjWrapper) == 14);

struct SmallString {
	char* ptr;       // 8B
	uint16_t length; // 2B 我们最大就支持 2^16 长度的字符串,够用了
	char prefix[4];  // 4B
} __attribute__((packed));

static_assert(sizeof(SmallString) == 14);

class CompactObj {
public:
	enum Tag : uint8_t {
		// 0-14 is reserved for inline lengths of string type.
		INT_TAG = 15,
		SMALL_STR_TAG = 16,
		ROBJ_TAG = 17,
		EXTERNAL_TAG = 18,
		JSON_TAG = 19,
		SBF_TAG = 20,
		NULL_TAG = 31,
	};

	CompactObj();
	explicit CompactObj(std::string_view str);
	explicit CompactObj(int64_t val);
	~CompactObj();

	CompactObj(const CompactObj& other);
	CompactObj& operator=(const CompactObj& other);
	CompactObj(CompactObj&& other) noexcept;
	CompactObj& operator=(CompactObj&& other) noexcept;

	static CompactObj fromString(std::string_view str);
	static CompactObj fromInt(int64_t val);
	static CompactObj fromKey(std::string_view key);

	bool operator==(const CompactObj& other) const;
	bool operator!=(const CompactObj& other) const;

	bool isNull() const;
	bool isInt() const;
	bool isString() const;
	bool isHash() const;
	bool isSet() const;
	bool isList() const;
	bool isZset() const;

	std::optional<std::string_view> tryToString() const;
	std::optional<int64_t> tryToInt() const;
	std::string toString() const;
	int64_t asInt() const;

	uint8_t getType() const;
	uint8_t getEncoding() const;
	size_t size() const;

	uint8_t getTag() const;
	uint8_t getFlag() const;

	std::string_view getStringView() const;
	int64_t getIntValue() const;

	template<typename T>
	T* getObj() const {
		if (getTag() != ROBJ_TAG) {
			return nullptr;
		}
		return static_cast<T*>(u_.robj.inner_obj_);
	}

	template<typename T>
	void setObj(T* obj) {
		if (getTag() != ROBJ_TAG) {
			return;
		}
		if (u_.robj.inner_obj_ != nullptr) {
			::operator delete(u_.robj.inner_obj_);
		}
		u_.robj.inner_obj_ = obj;
	}

	static CompactObj fromHash();
	static CompactObj fromSet();
	static CompactObj fromList();
	static CompactObj fromZset();

	void setHash();
	void setSet();
	void setList();
	void setZset();




private:
	void setTag(uint8_t tag);
	void setFlag(uint8_t flag);
	void clear();
	void setString(std::string_view str);
	void setStringKey(std::string_view str);
	void setInt(int64_t val);
	void setInlineString(std::string_view str);
	void setSmallString(std::string_view str);
	void freeSmallString();
	std::string_view getRawStringView() const;

	uint8_t taglen_;           // for inline str len & TYPE
	uint8_t flag_;             // bitmap
	union U {                  // <= 14B
		char data[kInlineLen]; // inline
		SmallString small_str;
		RobjWrapper robj;
		int64_t ival __attribute__((packed)); // 这里浪费了一些不过没有关系
	} u_;
	static_assert(sizeof(u_) == 14);
} __attribute__((packed));

static_assert(sizeof(CompactObj) == 16);
