#include "bencode.h"

#include <cerrno>
#include <cstdlib>

#include "absl/strings/str_format.h"

#define CONCAT(lhs, next_expression)                                           \
    lhs = std::move(lhs).bind([&](const std::string& prefix) -> EncodeResult { \
        ASSIGN_OR_RAISE(auto suffix, (next_expression));                       \
        return prefix + suffix;                                                \
    })

namespace ryu {
namespace bencode {

EncodeResult BencodeInteger::encode() const { return absl::StrFormat("i%de", val_); }
EncodeResult BencodeInteger::json() const { return absl::StrFormat("%d", val_); }

EncodeResult BencodeString::encode() const { return absl::StrFormat("%d:%s", val_.size(), val_); }
EncodeResult BencodeString::json() const {
    return absl::StrFormat("\"%s\"", val_);  // TODO: escaping
}

EncodeResult BencodeString::encode(const std::string& str) {
    return absl::StrFormat("%d:%s", str.size(), str);
}

EncodeResult BencodeList::encode() const {
    auto ret = EncodeResult{"l"};
    for (auto& obj : list_) {
        CONCAT(ret, obj->encode());
    }
    CONCAT(ret, EncodeResult{"e"});
    return ret;
}

EncodeResult BencodeList::json() const {
    auto ret = EncodeResult("[");
    bool first = true;
    for (auto& obj : list_) {
        if (!first) {
            CONCAT(ret, EncodeResult{","});
        } else {
            first = false;
        }
        CONCAT(ret, obj->json());
    }
    CONCAT(ret, EncodeResult{"]"});
    return ret;
}

EncodeResult BencodeMap::encode() const {
    auto ret = EncodeResult("d");
    for (const auto& it : map_) {
        CONCAT(ret, BencodeString::encode(it.first));
        CONCAT(ret, it.second->encode());
    }
    CONCAT(ret, EncodeResult{"e"});
    return ret;
}

EncodeResult BencodeMap::json() const {
    auto ret = EncodeResult("{");
    bool first = true;
    for (const auto& it : map_) {
        if (!first) {
            CONCAT(ret, EncodeResult{","});
        } else {
            first = false;
        }
        CONCAT(ret, EncodeResult{"\"" + it.first + "\""});
        CONCAT(ret, EncodeResult{":"});
        CONCAT(ret, it.second->json());
    }
    CONCAT(ret, EncodeResult{"}"});
    return ret;
}

Result<std::unique_ptr<BencodeObject>> BencodeObject::parse(const std::string& str,
                                                            size_t* idx_inout) {
    if (*idx_inout >= str.size())
        return Err(absl::StrFormat("expecting object but end of input reached"));
    switch (str[*idx_inout]) {
        case 'i':
            return BencodeInteger::parse(str, idx_inout);
        case 'd':
            return BencodeMap::parse(str, idx_inout);
        case 'l':
            return BencodeList::parse(str, idx_inout);
        default: {
            if (str[*idx_inout] < '0' || str[*idx_inout] > '9') {
                return Err(
                    absl::StrFormat("invalid object type %c at %d", str[*idx_inout], *idx_inout));
            }
            return BencodeString::parse(str, idx_inout);
        }
    }
}

Result<std::unique_ptr<BencodeInteger>> BencodeInteger::parse(const std::string& str,
                                                              size_t* idx_inout) {
    size_t start_idx = *idx_inout;
    auto epos = str.find('e', *idx_inout);
    if (epos == std::string::npos) {
        return Err(absl::StrFormat("no ending mark found for integer at: %d", *idx_inout));
    } else {
        size_t len = epos - *idx_inout - 1;
        const auto n_str = str.substr(*idx_inout + 1, len);
        char* ptr;
        errno = 0;
        int64_t n = strtoll(n_str.c_str(), &ptr, 10);
        if (*ptr != '\0')
            return Err(absl::StrFormat("expecting integer but something else received: %s at %d",
                                       n_str, *idx_inout));
        if (errno == ERANGE)
            return Err(absl::StrFormat("integer value out of range: %s at %d", n_str, *idx_inout));
        *idx_inout = epos + 1;
        return std::make_unique<BencodeInteger>(n, str.substr(start_idx, *idx_inout - start_idx));
    }
}

Result<std::unique_ptr<BencodeString>> BencodeString::parse(const std::string& str,
                                                            size_t* idx_inout) {
    size_t start_idx = *idx_inout;
    auto epos = str.find(':', *idx_inout);
    if (epos == std::string::npos)
        return Err(absl::StrFormat("cannot find `:` mark for string at: %d", *idx_inout));

    std::string len_s = str.substr(*idx_inout, epos - *idx_inout);
    errno = 0;
    char* ptr;
    int64_t len = strtoll(len_s.c_str(), &ptr, 10);
    if (*ptr != '\0')
        return Err(absl::StrFormat("cannot parse string length: %s at %d", len_s, *idx_inout));
    if (errno == ERANGE || len < 0)
        return Err(absl::StrFormat("string length out of range: %s at %d", len_s, *idx_inout));

    std::string value = str.substr(epos + 1, len);
    if (value.size() != static_cast<uint64_t>(len))
        return Err(absl::StrFormat("string ends prematurely at %d, expecting %d, has %d: %s",
                                   *idx_inout, len, value.size(), value));

    *idx_inout = epos + len + 1;
    return std::make_unique<BencodeString>(value, str.substr(start_idx, *idx_inout - start_idx));
}

Result<std::unique_ptr<BencodeList>> BencodeList::parse(const std::string& str, size_t* idx_inout) {
    size_t start_idx = *idx_inout;
    auto ret = std::make_unique<BencodeList>();
    ++*idx_inout;
    while (true) {
        if (*idx_inout >= str.size())
            return Err(absl::StrFormat("list at %d ends prematurely", start_idx));
        if (str[*idx_inout] == 'e') {
            ++*idx_inout;
            break;
        }
        ASSIGN_OR_RAISE(auto next, BencodeObject::parse(str, idx_inout));
        ret->append(std::move(next));
    }
    ret->orig_data_ = str.substr(start_idx, *idx_inout - start_idx);
    return ret;
}

Result<std::unique_ptr<BencodeMap>> BencodeMap::parse(const std::string& str, size_t* idx_inout) {
    size_t start_idx = *idx_inout;
    auto ret = std::make_unique<BencodeMap>();
    ++*idx_inout;
    while (true) {
        if (*idx_inout >= str.size())
            return Err(absl::StrFormat("map at %d ends prematurely", start_idx));
        if (str[*idx_inout] == 'e') {
            ++*idx_inout;
            break;
        } else if ('0' <= str[*idx_inout] && str[*idx_inout] <= '9') {
            ASSIGN_OR_RAISE(auto key, BencodeString::parse(str, idx_inout));
            if (*idx_inout >= str.size())
                return Err(absl::StrFormat("map at %d ends prematurely", start_idx));
            ASSIGN_OR_RAISE(auto value, BencodeObject::parse(str, idx_inout));

            auto replaced = ret->set(key->value(), std::move(value));
            if (replaced != nullptr)
                return Err(
                    absl::StrFormat("duplicated key %s in map at %d", key->value(), start_idx));
        } else {
            return Err(absl::StrFormat("map at %d requires a string-type key at %d", start_idx,
                                       *idx_inout));
        }
    }
    ret->orig_data_ = str.substr(start_idx, *idx_inout - start_idx);
    return ret;
}

}  // namespace bencode
}  // namespace ryu