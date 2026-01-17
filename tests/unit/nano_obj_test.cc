#include <gtest/gtest.h>
#include "core/nano_obj.h"

class NanoObjTest : public ::testing::Test {
protected:
};

TEST_F(NanoObjTest, NullConstruction) {
    NanoObj v;
    EXPECT_TRUE(v.isNull());
    EXPECT_FALSE(v.isInt());
    EXPECT_FALSE(v.isString());
    EXPECT_EQ(v.getType(), OBJ_STRING);
    EXPECT_EQ(v.getEncoding(), OBJ_ENCODING_RAW);
}

TEST_F(NanoObjTest, IntInline) {
    NanoObj v = NanoObj::fromInt(42);
    EXPECT_FALSE(v.isNull());
    EXPECT_TRUE(v.isInt());
    EXPECT_FALSE(v.isString());

    EXPECT_EQ(v.asInt(), 42);

    auto opt_int = v.tryToInt();
    ASSERT_TRUE(opt_int.has_value());
    EXPECT_EQ(opt_int.value(), 42);

    auto opt_str = v.tryToString();
    EXPECT_FALSE(opt_str.has_value());

    std::string s = v.toString();
    EXPECT_EQ(s, "42");

    EXPECT_EQ(v.getType(), OBJ_STRING);
    EXPECT_EQ(v.getEncoding(), OBJ_ENCODING_INT);
}

TEST_F(NanoObjTest, NegativeInt) {
    NanoObj v = NanoObj::fromInt(-100);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), -100);
    EXPECT_EQ(v.toString(), "-100");
}

TEST_F(NanoObjTest, LargeInt) {
    int64_t large_int = 9000000000000000000LL;
    NanoObj v = NanoObj::fromInt(large_int);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), large_int);
    EXPECT_EQ(v.toString(), std::to_string(large_int));
}

TEST_F(NanoObjTest, InlineString) {
    NanoObj v = NanoObj::fromString("hello");
    EXPECT_FALSE(v.isNull());
    EXPECT_FALSE(v.isInt());
    EXPECT_TRUE(v.isString());

    auto opt_str = v.tryToString();
    ASSERT_TRUE(opt_str.has_value());
    EXPECT_EQ(opt_str.value(), "hello");

    auto opt_int = v.tryToInt();
    EXPECT_FALSE(opt_int.has_value());

    std::string s = v.toString();
    EXPECT_EQ(s, "hello");

    EXPECT_EQ(v.getType(), OBJ_STRING);
    EXPECT_EQ(v.getEncoding(), OBJ_ENCODING_EMBSTR);
    EXPECT_EQ(v.size(), 5);
}

TEST_F(NanoObjTest, InlineStringMaxLength) {
    std::string str(13, 'a');
    NanoObj v = NanoObj::fromString(str);
    EXPECT_TRUE(v.isString());

    auto opt_str = v.tryToString();
    ASSERT_TRUE(opt_str.has_value());
    EXPECT_EQ(opt_str.value().size(), 13);
}

TEST_F(NanoObjTest, SmallString) {
    std::string long_str(100, 'x');
    NanoObj v = NanoObj::fromString(long_str);
    EXPECT_FALSE(v.isNull());
    EXPECT_FALSE(v.isInt());
    EXPECT_TRUE(v.isString());

    std::string s = v.toString();
    EXPECT_EQ(s, long_str);

    EXPECT_EQ(v.getType(), OBJ_STRING);
    EXPECT_EQ(v.getEncoding(), OBJ_ENCODING_RAW);
    EXPECT_EQ(v.size(), 100);
}

TEST_F(NanoObjTest, SmallStringWithLength) {
    NanoObj v = NanoObj::fromString("hello world, this is a long string");
    EXPECT_TRUE(v.isString());

    std::string s = v.toString();
    EXPECT_EQ(s, "hello world, this is a long string");
}

TEST_F(NanoObjTest, MoveConstructor) {
    NanoObj v1 = NanoObj::fromInt(42);
    NanoObj v2 = std::move(v1);

    EXPECT_EQ(v2.asInt(), 42);
    EXPECT_TRUE(v1.isNull());
}

TEST_F(NanoObjTest, MoveAssignment) {
    NanoObj v1 = NanoObj::fromString("hello");
    NanoObj v2;

    v2 = std::move(v1);

    EXPECT_EQ(v2.tryToString().value(), "hello");
    EXPECT_TRUE(v1.isNull());
}

TEST_F(NanoObjTest, TypeAndEncodingConsistency) {
    NanoObj v_int = NanoObj::fromInt(42);
    EXPECT_EQ(v_int.getType(), OBJ_STRING);
    EXPECT_EQ(v_int.getEncoding(), OBJ_ENCODING_INT);

    NanoObj v_small = NanoObj::fromString("hi");
    EXPECT_EQ(v_small.getType(), OBJ_STRING);
    EXPECT_EQ(v_small.getEncoding(), OBJ_ENCODING_EMBSTR);

    NanoObj v_heap = NanoObj::fromString("this is a longer string that goes to heap");
    EXPECT_EQ(v_heap.getType(), OBJ_STRING);
    EXPECT_EQ(v_heap.getEncoding(), OBJ_ENCODING_RAW);

    NanoObj v_null;
    EXPECT_EQ(v_null.getType(), OBJ_STRING);
    EXPECT_EQ(v_null.getEncoding(), OBJ_ENCODING_RAW);
}

TEST_F(NanoObjTest, DestructorCleanup) {
    {
        NanoObj v1 = NanoObj::fromString("long string to be freed");
        NanoObj v2 = std::move(v1);
    }
}

TEST_F(NanoObjTest, ConstructorFromStringView) {
    NanoObj v(std::string_view("test"));
    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "test");
}

TEST_F(NanoObjTest, ConstructorFromInt) {
    NanoObj v(123);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), 123);
}

TEST_F(NanoObjTest, EmptyString) {
    NanoObj v = NanoObj::fromString("");
    EXPECT_TRUE(v.isString());

    std::string s = v.toString();
    EXPECT_EQ(s, "");
    EXPECT_EQ(s.size(), 0);
}

TEST_F(NanoObjTest, Zero) {
    NanoObj v = NanoObj::fromInt(0);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), 0);
    EXPECT_EQ(v.toString(), "0");
}

TEST_F(NanoObjTest, OneByteString) {
    NanoObj v = NanoObj::fromString("a");
    EXPECT_TRUE(v.isString());

    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "a");
}

TEST_F(NanoObjTest, ThirteenByteString) {
    NanoObj v = NanoObj::fromString("1234567890123");
    EXPECT_TRUE(v.isString());

    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "1234567890123");
}

TEST_F(NanoObjTest, FourteenByteString) {
    NanoObj v = NanoObj::fromString("12345678901234");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.getTag(), 14);

    std::string s = v.toString();
    EXPECT_EQ(s, "12345678901234");
}

TEST_F(NanoObjTest, FifteenByteString) {
    NanoObj v = NanoObj::fromString("123456789012345");
    EXPECT_TRUE(v.isString());
    EXPECT_TRUE(v.getTag() == NanoObj::SMALL_STR_TAG);

    std::string s = v.toString();
    EXPECT_EQ(s, "123456789012345");
}

TEST_F(NanoObjTest, IntCanBeConvertedToString) {
    NanoObj v = NanoObj::fromInt(12345);
    std::string s = v.toString();
    EXPECT_EQ(s, "12345");
}

TEST_F(NanoObjTest, StringCanBeConvertedToString) {
    NanoObj v = NanoObj::fromString("test string");
    std::string s = v.toString();
    EXPECT_EQ(s, "test string");
}

TEST_F(NanoObjTest, NullConvertedToString) {
    NanoObj v;
    std::string s = v.toString();
    EXPECT_EQ(s, "");
}

TEST_F(NanoObjTest, LargeSmallString) {
    std::string large_str(1000, 'x');
    NanoObj v = NanoObj::fromString(large_str);

    std::string s = v.toString();
    EXPECT_EQ(s, large_str);
    EXPECT_EQ(s.size(), 1000);
}

TEST_F(NanoObjTest, OverwriteInPlace) {
    NanoObj v = NanoObj::fromInt(100);
    EXPECT_EQ(v.asInt(), 100);

    v = NanoObj::fromString("hello");
    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "hello");
}

TEST_F(NanoObjTest, StringTag) {
    NanoObj v = NanoObj::fromInt(100);
    v = NanoObj::fromString("test string");
    EXPECT_TRUE(v.isString());
    EXPECT_FALSE(v.isNull());
}

TEST_F(NanoObjTest, SizeForInt) {
    NanoObj v = NanoObj::fromInt(12345);
    EXPECT_GT(v.size(), 0);
}

TEST_F(NanoObjTest, SizeForInlineString) {
    NanoObj v = NanoObj::fromString("hello");
    EXPECT_EQ(v.size(), 5);
}

TEST_F(NanoObjTest, SizeForSmallString) {
    NanoObj v = NanoObj::fromString("this is a longer string");
    EXPECT_GT(v.size(), 13);
}

TEST_F(NanoObjTest, NanoObjSize) {
    EXPECT_EQ(sizeof(NanoObj), 16);
}
