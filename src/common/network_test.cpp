
#include "network.h"

#include <gtest/gtest.h>

#include <memory>

using ryu::Err;
using ryu::Result;
using ryu::net::AddressType;
using ryu::net::IpAddress;

TEST(NetworkTest, Ipv4) {
    ASSERT_OK_AND_ASSIGN(auto parsed, IpAddress::FromString("192.168.254.168"));
    EXPECT_EQ(parsed.Type(), AddressType::IPv4);
    EXPECT_EQ(parsed.ToString().TakeOrRaise(), "192.168.254.168");
}

TEST(NetworkTest, Ipv6) {
    ASSERT_OK_AND_ASSIGN(auto parsed, IpAddress::FromString("fe80::dead:beef"));
    EXPECT_EQ(parsed.Type(), AddressType::IPv6);
    EXPECT_EQ(parsed.ToString().TakeOrRaise(), "fe80::dead:beef");
}

TEST(NetworkTest, Ipv4InV6Form) {
    ASSERT_OK_AND_ASSIGN(auto parsed, IpAddress::FromString("::ffff:10.0.0.6"));
    EXPECT_EQ(parsed.Type(), AddressType::IPv4);
    EXPECT_EQ(parsed.ToString().TakeOrRaise(), "10.0.0.6");
}
