#include <gtest/gtest.h>
#include <cstdint>
#include <optional>
#include <string>
#include "core/nano_obj.h"

class NanoObjTest : public ::testing::Test {
protected:
};

TEST_F(NanoObjTest, NullConstruction) {
	NanoObj v;
	EXPECT_TRUE(v.IsNull());
	EXPECT_FALSE(v.IsInt());
	EXPECT_FALSE(v.IsString());
	EXPECT_EQ(v.GetType(), OBJ_STRING);
	EXPECT_EQ(v.GetEncoding(), OBJ_ENCODING_RAW);
}

TEST_F(NanoObjTest, IntInline) {
	NanoObj v = NanoObj::FromInt(42);
	EXPECT_FALSE(v.IsNull());
	EXPECT_TRUE(v.IsInt());
	EXPECT_FALSE(v.IsString());

	EXPECT_EQ(v.AsInt(), 42);

	auto opt_int = v.TryToInt();
	ASSERT_TRUE(opt_int.has_value());
	EXPECT_EQ(opt_int, std::optional<int64_t> {42});

	auto opt_str = v.TryToString();
	EXPECT_FALSE(opt_str.has_value());

	std::string s = v.ToString();
	EXPECT_EQ(s, "42");

	EXPECT_EQ(v.GetType(), OBJ_STRING);
	EXPECT_EQ(v.GetEncoding(), OBJ_ENCODING_INT);
}

TEST_F(NanoObjTest, NegativeInt) {
	NanoObj v = NanoObj::FromInt(-100);
	EXPECT_TRUE(v.IsInt());
	EXPECT_EQ(v.AsInt(), -100);
	EXPECT_EQ(v.ToString(), "-100");
}

TEST_F(NanoObjTest, LargeInt) {
	int64_t large_int = 9000000000000000000LL;
	NanoObj v = NanoObj::FromInt(large_int);
	EXPECT_TRUE(v.IsInt());
	EXPECT_EQ(v.AsInt(), large_int);
	EXPECT_EQ(v.ToString(), std::to_string(large_int));
}

TEST_F(NanoObjTest, InlineString) {
	NanoObj v = NanoObj::FromString("hello");
	EXPECT_FALSE(v.IsNull());
	EXPECT_FALSE(v.IsInt());
	EXPECT_TRUE(v.IsString());

	auto opt_str = v.TryToString();
	ASSERT_TRUE(opt_str.has_value());
	EXPECT_EQ(opt_str, std::optional<std::string> {"hello"});

	auto opt_int = v.TryToInt();
	EXPECT_FALSE(opt_int.has_value());

	std::string s = v.ToString();
	EXPECT_EQ(s, "hello");

	EXPECT_EQ(v.GetType(), OBJ_STRING);
	EXPECT_EQ(v.GetEncoding(), OBJ_ENCODING_EMBSTR);
	EXPECT_EQ(v.Size(), 5);
}

TEST_F(NanoObjTest, InlineStringMaxLength) {
	std::string str(13, 'a');
	NanoObj v = NanoObj::FromString(str);
	EXPECT_TRUE(v.IsString());

	auto opt_str = v.TryToString();
	ASSERT_TRUE(opt_str.has_value());
	EXPECT_EQ(opt_str, std::optional<std::string> {str});
}

TEST_F(NanoObjTest, SmallString) {
	std::string long_str(100, 'x');
	NanoObj v = NanoObj::FromString(long_str);
	EXPECT_FALSE(v.IsNull());
	EXPECT_FALSE(v.IsInt());
	EXPECT_TRUE(v.IsString());

	std::string s = v.ToString();
	EXPECT_EQ(s, long_str);

	EXPECT_EQ(v.GetType(), OBJ_STRING);
	EXPECT_EQ(v.GetEncoding(), OBJ_ENCODING_RAW);
	EXPECT_EQ(v.Size(), 100);
}

TEST_F(NanoObjTest, SmallStringWithLength) {
	NanoObj v = NanoObj::FromString("hello world, this is a long string");
	EXPECT_TRUE(v.IsString());

	std::string s = v.ToString();
	EXPECT_EQ(s, "hello world, this is a long string");
}

TEST_F(NanoObjTest, MoveConstructor) {
	NanoObj v1 = NanoObj::FromInt(42);
	NanoObj v2 = std::move(v1);

	EXPECT_EQ(v2.AsInt(), 42);
	// NOLINTNEXTLINE(bugprone-use-after-move) - NanoObj keeps a valid moved-from null state.
	EXPECT_TRUE(v1.IsNull());
}

TEST_F(NanoObjTest, MoveAssignment) {
	NanoObj v1 = NanoObj::FromString("hello");
	NanoObj v2;

	v2 = std::move(v1);

	EXPECT_EQ(v2.TryToString(), std::optional<std::string> {"hello"});
	// NOLINTNEXTLINE(bugprone-use-after-move) - NanoObj keeps a valid moved-from null state.
	EXPECT_TRUE(v1.IsNull());
}

TEST_F(NanoObjTest, TypeAndEncodingConsistency) {
	NanoObj v_int = NanoObj::FromInt(42);
	EXPECT_EQ(v_int.GetType(), OBJ_STRING);
	EXPECT_EQ(v_int.GetEncoding(), OBJ_ENCODING_INT);

	NanoObj v_small = NanoObj::FromString("hi");
	EXPECT_EQ(v_small.GetType(), OBJ_STRING);
	EXPECT_EQ(v_small.GetEncoding(), OBJ_ENCODING_EMBSTR);

	NanoObj v_heap = NanoObj::FromString("this is a longer string that goes to heap");
	EXPECT_EQ(v_heap.GetType(), OBJ_STRING);
	EXPECT_EQ(v_heap.GetEncoding(), OBJ_ENCODING_RAW);

	NanoObj v_null;
	EXPECT_EQ(v_null.GetType(), OBJ_STRING);
	EXPECT_EQ(v_null.GetEncoding(), OBJ_ENCODING_RAW);
}

TEST_F(NanoObjTest, DestructorCleanup) {
	{
		NanoObj v1 = NanoObj::FromString("long string to be freed");
		NanoObj v2 = std::move(v1);
	}
}

TEST_F(NanoObjTest, ConstructorFromStringView) {
	NanoObj v(std::string_view("test"));
	auto opt = v.TryToString();
	ASSERT_TRUE(opt.has_value());
	EXPECT_EQ(opt, std::optional<std::string> {"test"});
}

TEST_F(NanoObjTest, ConstructorFromInt) {
	NanoObj v(123);
	EXPECT_TRUE(v.IsInt());
	EXPECT_EQ(v.AsInt(), 123);
}

TEST_F(NanoObjTest, EmptyString) {
	NanoObj v = NanoObj::FromString("");
	EXPECT_TRUE(v.IsString());

	std::string s = v.ToString();
	EXPECT_EQ(s, "");
	EXPECT_EQ(s.size(), 0);
}

TEST_F(NanoObjTest, Zero) {
	NanoObj v = NanoObj::FromInt(0);
	EXPECT_TRUE(v.IsInt());
	EXPECT_EQ(v.AsInt(), 0);
	EXPECT_EQ(v.ToString(), "0");
}

TEST_F(NanoObjTest, OneByteString) {
	NanoObj v = NanoObj::FromString("a");
	EXPECT_TRUE(v.IsString());

	auto opt = v.TryToString();
	ASSERT_TRUE(opt.has_value());
	EXPECT_EQ(opt, std::optional<std::string> {"a"});
}

TEST_F(NanoObjTest, ThirteenByteString) {
	NanoObj v = NanoObj::FromString("1234567890123");
	EXPECT_TRUE(v.IsString());

	auto opt = v.TryToString();
	ASSERT_TRUE(opt.has_value());
	EXPECT_EQ(opt, std::optional<std::string> {"1234567890123"});
}

TEST_F(NanoObjTest, FourteenByteString) {
	NanoObj v = NanoObj::FromString("12345678901234");
	EXPECT_TRUE(v.IsString());
	EXPECT_EQ(v.GetTag(), 14);

	std::string s = v.ToString();
	EXPECT_EQ(s, "12345678901234");
}

TEST_F(NanoObjTest, FifteenByteString) {
	NanoObj v = NanoObj::FromString("123456789012345");
	EXPECT_TRUE(v.IsString());
	EXPECT_TRUE(v.GetTag() == NanoObj::SMALL_STR_TAG);

	std::string s = v.ToString();
	EXPECT_EQ(s, "123456789012345");
}

TEST_F(NanoObjTest, IntCanBeConvertedToString) {
	NanoObj v = NanoObj::FromInt(12345);
	std::string s = v.ToString();
	EXPECT_EQ(s, "12345");
}

TEST_F(NanoObjTest, StringCanBeConvertedToString) {
	NanoObj v = NanoObj::FromString("test string");
	std::string s = v.ToString();
	EXPECT_EQ(s, "test string");
}

TEST_F(NanoObjTest, NullConvertedToString) {
	NanoObj v;
	std::string s = v.ToString();
	EXPECT_EQ(s, "");
}

TEST_F(NanoObjTest, LargeSmallString) {
	std::string large_str(1000, 'x');
	NanoObj v = NanoObj::FromString(large_str);

	std::string s = v.ToString();
	EXPECT_EQ(s, large_str);
	EXPECT_EQ(s.size(), 1000);
}

TEST_F(NanoObjTest, OverwriteInPlace) {
	NanoObj v = NanoObj::FromInt(100);
	EXPECT_EQ(v.AsInt(), 100);

	v = NanoObj::FromString("hello");
	auto opt = v.TryToString();
	ASSERT_TRUE(opt.has_value());
	EXPECT_EQ(opt, std::optional<std::string> {"hello"});
}

TEST_F(NanoObjTest, StringTag) {
	NanoObj v = NanoObj::FromInt(100);
	v = NanoObj::FromString("test string");
	EXPECT_TRUE(v.IsString());
	EXPECT_FALSE(v.IsNull());
}

TEST_F(NanoObjTest, SizeForInt) {
	NanoObj v = NanoObj::FromInt(12345);
	EXPECT_GT(v.Size(), 0);
}

TEST_F(NanoObjTest, SizeForInlineString) {
	NanoObj v = NanoObj::FromString("hello");
	EXPECT_EQ(v.Size(), 5);
}

TEST_F(NanoObjTest, SizeForSmallString) {
	NanoObj v = NanoObj::FromString("this is a longer string");
	EXPECT_GT(v.Size(), 13);
}

TEST_F(NanoObjTest, NanoObjSize) {
	EXPECT_EQ(sizeof(NanoObj), 16);
}
