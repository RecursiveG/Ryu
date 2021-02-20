#include "network.h"

#include <endian.h>
#include <uv.h>

#include <cstdint>
#include <string>

#include "absl/strings/str_cat.h"
#include "result.h"

#define ERR(...) return Err(absl::StrCat(__VA_ARGS__))

namespace ryu::net {

// Supported formats
// - x:x:x::x
// - [x:x:x::x]
// - [x:x:x::x]:p
// - a.b.c.d
// - a.b.c.d:p
Result<ResultVoid, std::string> ParseAddrPort(std::string addr_str, sockaddr_storage* out,
                                              uint16_t default_port) {
    sa_family_t af = 0;
    const char* addr_s = nullptr;
    const char* port_s = nullptr;

    if (addr_str.empty()) {
        ERR("empty std::string");
    }
    if (addr_str[0] == '[') {
        size_t addr_epos = addr_str.find(']');
        if (addr_epos == std::string::npos) {
            ERR("missing ipv6 ending bracket");
        }
        addr_str[addr_epos] = '\0';

        size_t colon_pos = addr_str.find(':', addr_epos + 1);
        if (colon_pos == std::string::npos) {
            // case 2
            af = AF_INET6;
            addr_s = addr_str.c_str() + 1;
            port_s = nullptr;
        } else {
            // case 3
            af = AF_INET6;
            addr_s = addr_str.c_str() + 1;
            port_s = addr_str.c_str() + colon_pos + 1;
        }
    } else {
        size_t colon_first_pos = addr_str.find(':');
        size_t colon_last_pos = addr_str.rfind(':');
        if (colon_first_pos == std::string::npos && colon_last_pos == std::string::npos) {
            // case 4
            af = AF_INET;
            addr_s = addr_str.c_str();
            port_s = nullptr;
        } else if (colon_first_pos == colon_last_pos) {
            // case 5
            addr_str[colon_first_pos] = '\0';
            af = AF_INET;
            addr_s = addr_str.c_str();
            port_s = addr_str.c_str() + colon_first_pos + 1;
        } else {
            // case 1
            af = AF_INET6;
            addr_s = addr_str.c_str();
            port_s = nullptr;
        }
    }
    // cout << "addr_s=" << addr_s;
    // if (port_s != nullptr)
    //     cout << " port_s=" << port_s << endl;
    // else
    //     cout << " port_s=default" << endl;

    uint16_t port = htobe16(default_port);
    if (port_s != nullptr) {
        char* end_ptr = nullptr;
        unsigned long long p = strtoull(port_s, &end_ptr, 10);
        if (*end_ptr != '\0') {
            ERR("Failed to parse port ", port_s);
        }
        if (p > 65535) {
            ERR("Port out of range ", p);
        }
        port = htobe16(p);
    }

    constexpr size_t kIpStrBufLen = 64;
    *out = {};
    out->ss_family = af;
    if (af == AF_INET) {
        int retcode;
        sockaddr_in& addr = *(sockaddr_in*)out;
        char unparsed[kIpStrBufLen] = {};

        retcode = uv_inet_pton(AF_INET, addr_s, &addr.sin_addr);
        if (retcode != 0) {
            ERR("Failed to parse", addr_s);
        }
        addr.sin_port = port;
        retcode = uv_inet_ntop(AF_INET, &addr.sin_addr, unparsed, kIpStrBufLen);
        if (retcode != 0) {
            ERR("Failed to unparse", addr_s);
        }
        // cout << "IPv4 " << unparsed << " Port " << be16toh(addr.sin_port) << endl;
    } else {
        int retcode;
        sockaddr_in6& addr = *(sockaddr_in6*)out;
        char unparsed[kIpStrBufLen] = {};

        retcode = uv_inet_pton(AF_INET6, addr_s, &addr.sin6_addr);
        if (retcode != 0) {
            ERR("Failed to parse", addr_s);
        }
        addr.sin6_port = port;
        retcode = uv_inet_ntop(AF_INET6, &addr.sin6_addr, unparsed, kIpStrBufLen);
        if (retcode != 0) {
            ERR("Failed to unparse", addr_s);
        }
        // cout << "IPv6 " << unparsed << " Port " << be16toh(addr.sin6_port) << endl;
    }
    return {};
}

}  // namespace ryu::net