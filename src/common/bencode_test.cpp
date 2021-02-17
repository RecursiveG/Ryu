#include "bencode.h"

#include <gtest/gtest.h>

using namespace ryu::bencode;
using std::string;

#define PARSE(str)                                \
    ({                                            \
        size_t idx = 0;                           \
        auto i = BencodeObject::Parse(str, &idx); \
        if (!i) FAIL() << i.Error();              \
        ASSERT_EQ(str, i.Value()->Encode());      \
        std::move(i).TakeValue();                 \
    })

TEST(BencodeTest, ParseInt) {
    EXPECT_EQ(-42, PARSE("i-42e")->GetInt());
    EXPECT_EQ(0, PARSE("i0e")->GetInt());
    EXPECT_EQ(42, PARSE("i42e")->GetInt());
}

TEST(BencodeTest, ParseString) {
    EXPECT_EQ("", PARSE("0:")->GetString());
    EXPECT_EQ(std::string("\0", 1), PARSE(std::string("1:\0", 3))->GetString());
    EXPECT_EQ("hello, world", PARSE("12:hello, world")->GetString());
}

TEST(BencodeTest, ParseList) {
    ASSERT_EQ(0, PARSE("le")->Size());

    auto obj = PARSE("l3:foo3:bari42ee");
    ASSERT_EQ(3, obj->Size());
    EXPECT_EQ("foo", (*obj)[0].GetString());
    EXPECT_EQ("bar", (*obj)[1].GetString());
    EXPECT_EQ(42, (*obj)[2].GetInt());
}

TEST(BencodeTest, ParseMap) {
    ASSERT_EQ(0, PARSE("de")->Size());

    auto obj = PARSE("d3:fooi16e3:bar3:buze");
    ASSERT_EQ(2, obj->Size());
    EXPECT_EQ(16, (*obj)["foo"].GetInt());
    EXPECT_EQ("buz", (*obj)["bar"].GetString());
}

#define ENCODE(obj)                  \
    ({                               \
        auto e = obj.Encode();       \
        if (!e) FAIL() << e.Error(); \
        e.Value();                   \
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
    l1.Add(std::make_unique<BencodeInteger>(42));
    l1.Add(std::make_unique<BencodeString>("foo"));
    EXPECT_EQ("li42e3:fooe", ENCODE(l1));

    BencodeList l2;
    l2.Add(std::make_unique<BencodeList>());
    l2.Add(std::make_unique<BencodeMap>());
    EXPECT_EQ("lledee", ENCODE(l2));
}
TEST(BencodeTest, EncodeMap) {
    EXPECT_EQ("de", ENCODE(BencodeMap()));

    BencodeMap m1;
    EXPECT_EQ(true, m1.Set("foo", std::make_unique<BencodeInteger>(42)));
    EXPECT_EQ(true, m1.Set("bar", std::make_unique<BencodeString>("foo")));
    EXPECT_EQ("d3:fooi42e3:bar3:fooe", ENCODE(m1));

    BencodeMap m2;
    EXPECT_EQ(true, m2.Set("foo", std::make_unique<BencodeList>()));
    EXPECT_EQ(true, m2.Set("bar", std::make_unique<BencodeMap>()));
    EXPECT_EQ("d3:foole3:bardee", ENCODE(m2));

    BencodeMap m3;
    EXPECT_EQ(true, m3.Set("bar", std::make_unique<BencodeMap>()));
    EXPECT_EQ(true, m3.Set("foo", std::make_unique<BencodeList>()));
    EXPECT_EQ("d3:barde3:foolee", ENCODE(m3));
}
