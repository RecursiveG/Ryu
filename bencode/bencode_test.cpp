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
    EXPECT_EQ(-42, PARSE("i-42e")->try_int());
    EXPECT_EQ(0, PARSE("i0e")->try_int());
    EXPECT_EQ(42, PARSE("i42e")->try_int());
}

TEST(BencodeTest, ParseString) {
    EXPECT_EQ("", PARSE("0:")->try_string());
    EXPECT_EQ(std::string("\0", 1), PARSE(std::string("1:\0", 3))->try_string());
    EXPECT_EQ("hello, world", PARSE("12:hello, world")->try_string());
}

TEST(BencodeTest, ParseList) {
    ASSERT_EQ(0, PARSE("le")->try_size());

    auto obj = PARSE("l3:foo3:bari42ee");
    ASSERT_EQ(3, obj->try_size());
    EXPECT_EQ("foo", obj->try_string(0));
    EXPECT_EQ("bar", obj->try_string(1));
    EXPECT_EQ(42, obj->try_int(2));
}

TEST(BencodeTest, ParseMap) {
    ASSERT_EQ(0, PARSE("de")->try_size());

    auto obj = PARSE("d3:fooi16e3:bar3:buze");
    ASSERT_EQ(2, obj->try_size());
    EXPECT_EQ(16, obj->try_int("foo"));
    EXPECT_EQ("buz", obj->try_string("bar"));
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
    EXPECT_EQ(string("1:\0", 3), ENCODE(BencodeString(string("\0", 1))));
    EXPECT_EQ("5:hello", ENCODE(BencodeString("hello")));
}

TEST(BencodeTest, EncodeList) {
    EXPECT_EQ("le", ENCODE(BencodeList()));

    BencodeList l1;
    l1.append(std::make_unique<BencodeInteger>(42));
    l1.append(std::make_unique<BencodeString>("foo"));
    EXPECT_EQ("li42e3:fooe", ENCODE(l1));

    BencodeList l2;
    l2.append(std::make_unique<BencodeList>());
    l2.append(std::make_unique<BencodeMap>());
    EXPECT_EQ("lledee", ENCODE(l2));
}
TEST(BencodeTest, EncodeMap) {
    EXPECT_EQ("de", ENCODE(BencodeMap()));

    BencodeMap m1;
    EXPECT_EQ(nullptr, m1.set("foo", std::make_unique<BencodeInteger>(42)).get());
    EXPECT_EQ(nullptr, m1.set("bar", std::make_unique<BencodeString>("foo")).get());
    EXPECT_EQ("d3:bar3:foo3:fooi42ee", ENCODE(m1));

    BencodeMap m2;
    EXPECT_EQ(nullptr, m2.set("foo", std::make_unique<BencodeList>()).get());
    EXPECT_EQ(nullptr, m2.set("bar", std::make_unique<BencodeMap>()).get());
    EXPECT_EQ("d3:barde3:foolee", ENCODE(m2));

    BencodeMap m3;
    EXPECT_EQ(nullptr, m3.set("bar", std::make_unique<BencodeMap>()).get());
    EXPECT_EQ(nullptr, m3.set("foo", std::make_unique<BencodeList>()).get());
    EXPECT_EQ("d3:barde3:foolee", ENCODE(m3));
}