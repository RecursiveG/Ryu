#ifndef RYU_NETWORK_H
#define RYU_NETWORK_H

#include <arpa/inet.h>

#include <stdexcept>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "result.h"

namespace ryu {
namespace net {

enum class AddressType { IPv6, IPv4 };

// Represent both IPv4 and IPv6 address
class IpAddress {
  public:
    static constexpr size_t STORAGE_SIZE = 16;
    static_assert(STORAGE_SIZE >= sizeof(in_addr));
    static_assert(STORAGE_SIZE >= sizeof(in6_addr));

    static Result<IpAddress> FromString(absl::string_view ip) {
        IpAddress ret{};
        memset(ret.addr_, 0, STORAGE_SIZE);
        int success = inet_pton(AF_INET, ip.data(), ret.addr_);
        if (success == 1) {
            ret.type_ = AddressType::IPv4;
            return ret;
        }
        success = inet_pton(AF_INET6, ip.data(), ret.addr_);
        if (success == 1) {
            if (*reinterpret_cast<uint64_t*>(ret.addr_) == 0 &&
                *reinterpret_cast<uint16_t*>(&ret.addr_[8]) == 0 &&
                *reinterpret_cast<uint16_t*>(&ret.addr_[10]) == 0xFFFFU) {
                // IPv4 in IPv6 format
                uint32_t val = *reinterpret_cast<uint32_t*>(&ret.addr_[12]);
                memset(ret.addr_, 0, STORAGE_SIZE);
                *reinterpret_cast<uint32_t*>(ret.addr_) = val;
                ret.type_ = AddressType::IPv4;
            } else {
                ret.type_ = AddressType::IPv6;
            }
            return ret;
        }
        return Err(absl::StrCat("failed to parse ip address \"%s\"", ip));
    }

    [[nodiscard]] AddressType Type() const { return type_; }
    [[nodiscard]] int AddressFamily() const {
        switch (type_) {
            case AddressType::IPv6:
                return AF_INET6;
            case AddressType::IPv4:
                return AF_INET;
            default:
                throw std::runtime_error("bug");
        }
    };
    [[nodiscard]] Result<std::string> ToString() const {
        char ret[INET6_ADDRSTRLEN];
        const char* err_val = inet_ntop(AddressFamily(), addr_, ret, INET6_ADDRSTRLEN);
        if (err_val == nullptr) {
            RAISE_ERRNO("failed to convert ip to string");
        }
        return {ret};
    }

  private:
    AddressType type_;
    // can be reinterpret_cast to in_addr or in6_addr
    uint8_t addr_[STORAGE_SIZE];
};

}  // namespace net
}  // namespace ryu

#endif  // RYU_NETWORK_H
