#include "bencode.h"

#include <cerrno>
#include <cstdlib>

#include "absl/strings/str_format.h"

namespace ryu::bencode {

EncodeResult BencodeInteger::encode() const {
    return EncodeResult::Ok(absl::StrFormat("i%de", val_));
}
EncodeResult BencodeInteger::json() const { return EncodeResult::Ok(absl::StrFormat("%d", val_)); }

EncodeResult BencodeString::encode() const {
    return EncodeResult::Ok(absl::StrFormat("%d:%s", val_.size(), val_));
}
EncodeResult BencodeString::json() const {
    return EncodeResult::Ok(absl::StrFormat("\"%s\"", val_));  // TODO: escaping
}

EncodeResult BencodeString::encode(const std::string& str) {
    return EncodeResult::Ok(absl::StrFormat("%d:%s", str.size(), str));
}

EncodeResult BencodeList::encode() const {
    auto ret = EncodeResult::Ok("l");
    for (auto& obj : list_) {
        ret.concat([&] { return obj->encode(); });
    }
    ret.concat([] { return EncodeResult::Ok("e"); });
    return ret;
}

EncodeResult BencodeList::json() const {
    auto ret = EncodeResult::Ok("[");
    bool first = true;
    for (auto& obj : list_) {
        if (!first) {
            ret.concat([] { return EncodeResult::Ok(","); });
        } else {
            first = false;
        }
        ret.concat([&] { return obj->json(); });
    }
    ret.concat([] { return EncodeResult::Ok("]"); });
    return ret;
}

EncodeResult BencodeMap::encode() const {
    auto ret = EncodeResult::Ok("d");
    for (const auto& it : map_) {
        ret.concat([&] { return BencodeString::encode(it.first); });
        ret.concat([&] { return it.second->encode(); });
    }
    ret.concat([] { return EncodeResult::Ok("e"); });
    return ret;
}

EncodeResult BencodeMap::json() const {
    auto ret = EncodeResult::Ok("{");
    bool first = true;
    for (const auto& it : map_) {
        if (!first) {
            ret.concat([] { return EncodeResult::Ok(","); });
        } else {
            first = false;
        }
        ret.concat([&] { return EncodeResult::Ok("\"" + it.first + "\""); });
        ret.concat([] { return EncodeResult::Ok(":"); });
        ret.concat([&] { return it.second->json(); });
    }
    ret.concat([] { return EncodeResult::Ok("}"); });
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
        return std::make_unique<BencodeInteger>(n);
    }
}

Result<std::unique_ptr<BencodeString>> BencodeString::parse(const std::string& str,
                                                            size_t* idx_inout) {
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
    return std::make_unique<BencodeString>(value);
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
    return ret;
}
}  // namespace ryu::bencode