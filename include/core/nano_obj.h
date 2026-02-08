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
	void* inner_obj;
	uint32_t sz;
	uint8_t type;
	uint8_t encoding;
} __attribute__((packed));

static_assert(sizeof(RobjWrapper) == 14);

struct SmallString {
	char* ptr;       // 8B
	uint16_t length; // 2B 我们最大就支持 2^16 长度的字符串,够用了
	char prefix[4];  // 4B
} __attribute__((packed));

static_assert(sizeof(SmallString) == 14);

class NanoObj {
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

	NanoObj();
	explicit NanoObj(std::string_view str);
	explicit NanoObj(int64_t val);
	~NanoObj();

	NanoObj(const NanoObj& other);
	NanoObj& operator=(const NanoObj& other);
	NanoObj(NanoObj&& other) noexcept;
	NanoObj& operator=(NanoObj&& other) noexcept;

	static NanoObj FromString(std::string_view str);
	static NanoObj FromInt(int64_t val);
	static NanoObj FromKey(std::string_view key);

	// Backward-compatible wrappers (transitional).
	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromString(std::string_view str) {
		return FromString(str);
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromInt(int64_t val) {
		return FromInt(val);
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromKey(std::string_view key) {
		return FromKey(key);
	}

	char* PrepareStringBuffer(size_t len);
	void FinalizePreparedString();
	bool MaybeConvertToInt();

	bool operator==(const NanoObj& other) const;
	bool operator!=(const NanoObj& other) const;

	bool IsNull() const;
	bool IsInt() const;
	bool IsString() const;
	bool IsHash() const;
	bool IsSet() const;
	bool IsList() const;
	bool IsZset() const;

	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isNull() const {
		return IsNull();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isInt() const {
		return IsInt();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isString() const {
		return IsString();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isHash() const {
		return IsHash();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isSet() const {
		return IsSet();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isList() const {
		return IsList();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	bool isZset() const {
		return IsZset();
	}

	std::optional<std::string_view> TryToString() const;
	std::optional<int64_t> TryToInt() const;
	std::string ToString() const;
	int64_t AsInt() const;

	// NOLINTNEXTLINE(readability-identifier-naming)
	std::optional<std::string_view> tryToString() const {
		return TryToString();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	std::optional<int64_t> tryToInt() const {
		return TryToInt();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	std::string toString() const {
		return ToString();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	int64_t asInt() const {
		return AsInt();
	}

	uint8_t GetType() const;
	uint8_t GetEncoding() const;
	size_t Size() const;

	// NOLINTNEXTLINE(readability-identifier-naming)
	uint8_t getType() const {
		return GetType();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	uint8_t getEncoding() const {
		return GetEncoding();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	size_t size() const {
		return Size();
	}

	uint8_t GetTag() const;
	uint8_t GetFlag() const;

	// NOLINTNEXTLINE(readability-identifier-naming)
	uint8_t getTag() const {
		return GetTag();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	uint8_t getFlag() const {
		return GetFlag();
	}

	std::string_view GetStringView() const;
	int64_t GetIntValue() const;

	// NOLINTNEXTLINE(readability-identifier-naming)
	std::string_view getStringView() const {
		return GetStringView();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	int64_t getIntValue() const {
		return GetIntValue();
	}

	template <typename T>
	T* GetObj() const {
		return taglen == ROBJ_TAG ? static_cast<T*>(u.robj.inner_obj) : nullptr;
	}

	template <typename T>
	// NOLINTNEXTLINE(readability-identifier-naming)
	T* getObj() const {
		return GetObj<T>();
	}

	template <typename T>
	void SetObj(T* obj) {
		if (taglen != ROBJ_TAG) {
			return;
		}
		if (u.robj.inner_obj != nullptr) {
			FreeRobj();
		}
		u.robj.inner_obj = obj;
	}

	template <typename T>
	// NOLINTNEXTLINE(readability-identifier-naming)
	void setObj(T* obj) {
		SetObj(obj);
	}

	static NanoObj FromHash();
	static NanoObj FromSet();
	static NanoObj FromList();
	static NanoObj FromZset();

	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromHash() {
		return FromHash();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromSet() {
		return FromSet();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromList() {
		return FromList();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	static NanoObj fromZset() {
		return FromZset();
	}

	void SetHash();
	void SetSet();
	void SetList();
	void SetZset();

	// NOLINTNEXTLINE(readability-identifier-naming)
	void setHash() {
		SetHash();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	void setSet() {
		SetSet();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	void setList() {
		SetList();
	}
	// NOLINTNEXTLINE(readability-identifier-naming)
	void setZset() {
		SetZset();
	}

private:
	void SetTag(uint8_t tag);
	void SetFlag(uint8_t flag);
	void Clear();
	void FreeRobj();
	void FreeSmallString();
	void SetString(std::string_view str);
	void SetStringKey(std::string_view str);
	void SetInt(int64_t val);
	void SetInlineString(std::string_view str);
	void SetSmallString(std::string_view str);
	std::string_view GetRawStringView() const;

	// Helpers for copy/move/init (reduce duplication)
	void CopyFrom(const NanoObj& other);
	void MoveFrom(NanoObj& other) noexcept;
	void ResetToNull();
	void InitRobj(uint8_t type, uint8_t encoding);

	uint8_t taglen;           // for inline str len & TYPE
	uint8_t flag;             // bitmap
	union U {                  // <= 14B
		char data[kInlineLen]; // inline
		SmallString small_str;
		RobjWrapper robj;
		int64_t ival __attribute__((packed)); // 这里浪费了一些不过没有关系
	} u;
	static_assert(sizeof(u) == 14);
} __attribute__((packed));

static_assert(sizeof(NanoObj) == 16);
