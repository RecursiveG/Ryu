
#include "network.h"

#include <gtest/gtest.h>

#include <memory>

#include "result.h"

using ryu::net::AddressType;
using ryu::net::IpAddress;

#define ASSERT_OK_AND_ASSIGN(lhs, rhs) \
    lhs = ({                           \
        auto tmp = (rhs);              \
        ASSERT_TRUE(tmp.Ok());         \
        std::move(tmp).TakeValue();               \
    })

TEST(NetworkTest, Ipv4) {
    ASSERT_OK_AND_ASSIGN(auto parsed, IpAddress::FromString("192.168.254.168"));
    EXPECT_EQ(parsed.Type(), AddressType::IPv4);
    EXPECT_EQ(parsed.ToString().Expect(""), "192.168.254.168");

    parsed = IpAddress::FromBe32(htobe32(0x0a000001u));
    EXPECT_EQ(parsed.Type(), AddressType::IPv4);
    EXPECT_EQ(parsed.ToString().Expect(""), "10.0.0.1");
}

TEST(NetworkTest, Ipv6) {
    ASSERT_OK_AND_ASSIGN(auto parsed, IpAddress::FromString("fe80::dead:beef"));
    EXPECT_EQ(parsed.Type(), AddressType::IPv6);
    EXPECT_EQ(parsed.ToString().Expect(""), "fe80::dead:beef");

    uint8_t ip[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    parsed = IpAddress::FromBe128(ip);
    EXPECT_EQ(parsed.Type(), AddressType::IPv6);
    EXPECT_EQ(parsed.ToString().Expect(""), "::1");
}

TEST(NetworkTest, Ipv4InV6Form) {
    ASSERT_OK_AND_ASSIGN(auto parsed, IpAddress::FromString("::ffff:10.0.0.6"));
    EXPECT_EQ(parsed.Type(), AddressType::IPv4);
    EXPECT_EQ(parsed.ToString().Expect(""), "10.0.0.6");

    uint8_t ip[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 10, 0, 0, 1};
    parsed = IpAddress::FromBe128(ip);
    EXPECT_EQ(parsed.Type(), AddressType::IPv4);
    EXPECT_EQ(parsed.ToString().Expect(""), "10.0.0.1");
}
