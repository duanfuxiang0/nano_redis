#include <gtest/gtest.h>
#include "core/compact_obj.h"

class CompactObjTest : public ::testing::Test {
protected:
};

TEST_F(CompactObjTest, NullConstruction) {
    CompactObj v;
    EXPECT_TRUE(v.isNull());
    EXPECT_FALSE(v.isInt());
    EXPECT_FALSE(v.isString());
    EXPECT_EQ(v.getType(), OBJ_STRING);
    EXPECT_EQ(v.getEncoding(), OBJ_ENCODING_RAW);
}

TEST_F(CompactObjTest, IntInline) {
    CompactObj v = CompactObj::fromInt(42);
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

TEST_F(CompactObjTest, NegativeInt) {
    CompactObj v = CompactObj::fromInt(-100);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), -100);
    EXPECT_EQ(v.toString(), "-100");
}

TEST_F(CompactObjTest, LargeInt) {
    int64_t large_int = 9000000000000000000LL;
    CompactObj v = CompactObj::fromInt(large_int);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), large_int);
    EXPECT_EQ(v.toString(), std::to_string(large_int));
}

TEST_F(CompactObjTest, InlineString) {
    CompactObj v = CompactObj::fromString("hello");
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

TEST_F(CompactObjTest, InlineStringMaxLength) {
    std::string str(13, 'a');
    CompactObj v = CompactObj::fromString(str);
    EXPECT_TRUE(v.isString());

    auto opt_str = v.tryToString();
    ASSERT_TRUE(opt_str.has_value());
    EXPECT_EQ(opt_str.value().size(), 13);
}

TEST_F(CompactObjTest, SmallString) {
    std::string long_str(100, 'x');
    CompactObj v = CompactObj::fromString(long_str);
    EXPECT_FALSE(v.isNull());
    EXPECT_FALSE(v.isInt());
    EXPECT_TRUE(v.isString());

    std::string s = v.toString();
    EXPECT_EQ(s, long_str);

    EXPECT_EQ(v.getType(), OBJ_STRING);
    EXPECT_EQ(v.getEncoding(), OBJ_ENCODING_RAW);
    EXPECT_EQ(v.size(), 100);
}

TEST_F(CompactObjTest, SmallStringWithLength) {
    CompactObj v = CompactObj::fromString("hello world, this is a long string");
    EXPECT_TRUE(v.isString());

    std::string s = v.toString();
    EXPECT_EQ(s, "hello world, this is a long string");
}

TEST_F(CompactObjTest, MoveConstructor) {
    CompactObj v1 = CompactObj::fromInt(42);
    CompactObj v2 = std::move(v1);

    EXPECT_EQ(v2.asInt(), 42);
    EXPECT_TRUE(v1.isNull());
}

TEST_F(CompactObjTest, MoveAssignment) {
    CompactObj v1 = CompactObj::fromString("hello");
    CompactObj v2;

    v2 = std::move(v1);

    EXPECT_EQ(v2.tryToString().value(), "hello");
    EXPECT_TRUE(v1.isNull());
}

TEST_F(CompactObjTest, TypeAndEncodingConsistency) {
    CompactObj v_int = CompactObj::fromInt(42);
    EXPECT_EQ(v_int.getType(), OBJ_STRING);
    EXPECT_EQ(v_int.getEncoding(), OBJ_ENCODING_INT);

    CompactObj v_small = CompactObj::fromString("hi");
    EXPECT_EQ(v_small.getType(), OBJ_STRING);
    EXPECT_EQ(v_small.getEncoding(), OBJ_ENCODING_EMBSTR);

    CompactObj v_heap = CompactObj::fromString("this is a longer string that goes to heap");
    EXPECT_EQ(v_heap.getType(), OBJ_STRING);
    EXPECT_EQ(v_heap.getEncoding(), OBJ_ENCODING_RAW);

    CompactObj v_null;
    EXPECT_EQ(v_null.getType(), OBJ_STRING);
    EXPECT_EQ(v_null.getEncoding(), OBJ_ENCODING_RAW);
}

TEST_F(CompactObjTest, DestructorCleanup) {
    {
        CompactObj v1 = CompactObj::fromString("long string to be freed");
        CompactObj v2 = std::move(v1);
    }
}

TEST_F(CompactObjTest, ConstructorFromStringView) {
    CompactObj v(std::string_view("test"));
    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "test");
}

TEST_F(CompactObjTest, ConstructorFromInt) {
    CompactObj v(123);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), 123);
}

TEST_F(CompactObjTest, EmptyString) {
    CompactObj v = CompactObj::fromString("");
    EXPECT_TRUE(v.isString());

    std::string s = v.toString();
    EXPECT_EQ(s, "");
    EXPECT_EQ(s.size(), 0);
}

TEST_F(CompactObjTest, Zero) {
    CompactObj v = CompactObj::fromInt(0);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), 0);
    EXPECT_EQ(v.toString(), "0");
}

TEST_F(CompactObjTest, OneByteString) {
    CompactObj v = CompactObj::fromString("a");
    EXPECT_TRUE(v.isString());

    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "a");
}

TEST_F(CompactObjTest, ThirteenByteString) {
    CompactObj v = CompactObj::fromString("1234567890123");
    EXPECT_TRUE(v.isString());

    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "1234567890123");
}

TEST_F(CompactObjTest, FourteenByteString) {
    CompactObj v = CompactObj::fromString("12345678901234");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.getTag(), 14);

    std::string s = v.toString();
    EXPECT_EQ(s, "12345678901234");
}

TEST_F(CompactObjTest, FifteenByteString) {
    CompactObj v = CompactObj::fromString("123456789012345");
    EXPECT_TRUE(v.isString());
    EXPECT_TRUE(v.getTag() == CompactObj::SMALL_STR_TAG);

    std::string s = v.toString();
    EXPECT_EQ(s, "123456789012345");
}

TEST_F(CompactObjTest, IntCanBeConvertedToString) {
    CompactObj v = CompactObj::fromInt(12345);
    std::string s = v.toString();
    EXPECT_EQ(s, "12345");
}

TEST_F(CompactObjTest, StringCanBeConvertedToString) {
    CompactObj v = CompactObj::fromString("test string");
    std::string s = v.toString();
    EXPECT_EQ(s, "test string");
}

TEST_F(CompactObjTest, NullConvertedToString) {
    CompactObj v;
    std::string s = v.toString();
    EXPECT_EQ(s, "");
}

TEST_F(CompactObjTest, LargeSmallString) {
    std::string large_str(1000, 'x');
    CompactObj v = CompactObj::fromString(large_str);

    std::string s = v.toString();
    EXPECT_EQ(s, large_str);
    EXPECT_EQ(s.size(), 1000);
}

TEST_F(CompactObjTest, OverwriteInPlace) {
    CompactObj v = CompactObj::fromInt(100);
    EXPECT_EQ(v.asInt(), 100);

    v = CompactObj::fromString("hello");
    auto opt = v.tryToString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "hello");
}

TEST_F(CompactObjTest, StringTag) {
    CompactObj v = CompactObj::fromInt(100);
    v = CompactObj::fromString("test string");
    EXPECT_TRUE(v.isString());
    EXPECT_FALSE(v.isNull());
}

TEST_F(CompactObjTest, SizeForInt) {
    CompactObj v = CompactObj::fromInt(12345);
    EXPECT_GT(v.size(), 0);
}

TEST_F(CompactObjTest, SizeForInlineString) {
    CompactObj v = CompactObj::fromString("hello");
    EXPECT_EQ(v.size(), 5);
}

TEST_F(CompactObjTest, SizeForSmallString) {
    CompactObj v = CompactObj::fromString("this is a longer string");
    EXPECT_GT(v.size(), 13);
}

TEST_F(CompactObjTest, CompactObjSize) {
    EXPECT_EQ(sizeof(CompactObj), 16);
}
