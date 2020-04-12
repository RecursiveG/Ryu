#include "bencode.h"

#include <gtest/gtest.h>

using namespace ryu::bencode;
using std::string;

#define PARSE(str)                                \
    ({                                            \
        size_t idx = 0;                           \
        auto i = BencodeObject::parse(str, &idx); \
        if (!i) FAIL() << i.err();                \
        std::move(*i);                            \
    })

TEST(BencodeTest, ParseInt) {
    EXPECT_EQ(-42, PARSE("i-42e")->as_int());
    EXPECT_EQ(0, PARSE("i0e")->as_int());
    EXPECT_EQ(42, PARSE("i42e")->as_int());
}

TEST(BencodeTest, ParseString) {
    EXPECT_EQ("", PARSE("0:")->as_string());
    EXPECT_EQ(std::string("\0", 1), PARSE(std::string("1:\0", 3))->as_string());
    EXPECT_EQ("hello, world", PARSE("12:hello, world")->as_string());
}

TEST(BencodeTest, ParseList) {
    auto obj = PARSE("l3:foo3:bari42ee");
    ASSERT_EQ(3, obj->size());
    EXPECT_EQ("foo", obj->get<string>(0));
    EXPECT_EQ("bar", obj->get<string>(1));
    EXPECT_EQ(42, obj->get<int64_t>(2));
}

TEST(BencodeTest, ParseMap) {
    auto obj = PARSE("d3:fooi16e3:bar3:buze");
    ASSERT_EQ(2, obj->size());
    EXPECT_EQ(16, obj->get<int64_t>("foo"));
    EXPECT_EQ("buz", obj->get<string>("bar"));
}

#define ENCODE(obj)            \
    ({                         \
        auto e = obj.encode(); \
        if (!e) FAIL() << *e;  \
        *e;                    \
    })

TEST(BencodeTest, EncodeInt) {
    EXPECT_EQ("i-99e", ENCODE(BencodeInteger(-99)));
    EXPECT_EQ("i0e", ENCODE(BencodeInteger(0)));
    EXPECT_EQ("i42e", ENCODE(BencodeInteger(42)));
}

TEST(BencodeTest, EncodeString) {
    EXPECT_EQ("0:", ENCODE(BencodeString("")));
    EXPECT_EQ(string("1:\0",3), ENCODE(BencodeString(string("\0",1))));
    EXPECT_EQ("5:hello", ENCODE(BencodeString("hello")));
}

TEST(BencodeTest, EncodeList) {
    EXPECT_EQ("le", ENCODE(BencodeList()));
}
TEST(BencodeTest, EncodeMap) {
    EXPECT_EQ("de", ENCODE(BencodeMap()));
}