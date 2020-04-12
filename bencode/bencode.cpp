#include "bencode.h"

#include <cerrno>
#include <cstdlib>

#include "fmt/format.h"
namespace ryu {
namespace bencode {

EncodeResult BencodeObject::encode() const {
    return EncodeResult::Err("cannot encode base object");
}

EncodeResult BencodeInteger::encode() const { return EncodeResult::Ok(fmt::format("i{}e", val_)); }

EncodeResult BencodeString::encode() const {
    return EncodeResult::Ok(fmt::format("{}:{}", val_.size(), val_));
}

EncodeResult BencodeString::encode(const std::string& str) {
    return EncodeResult::Ok(fmt::format("{}:{}", str.size(), str));
}

EncodeResult BencodeList::encode() const {
    auto ret = EncodeResult::Ok("l");
    for (auto& obj : list_) {
        ret.concat([&] { return obj->encode(); });
    }
    ret.concat([] { return EncodeResult::Ok("e"); });
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

Result<std::unique_ptr<BencodeObject>> BencodeObject::parse(const std::string& str,
                                                            size_t* idx_inout) {
    const auto Err = [](std::string str) -> auto {
        return Result<std::unique_ptr<BencodeObject>>::Err(str);
    };
    const auto Ok = [](BencodeObject * obj) -> auto {
        return Result<std::unique_ptr<BencodeObject>>::Ok(std::unique_ptr<BencodeObject>(obj));
    };

    if (*idx_inout >= str.size())
        return Err(fmt::format("expecting object but end of input reached"));
    switch (str[*idx_inout]) {
        case 'i': {
            auto i = BencodeInteger::parse(str, idx_inout);
            if (i)
                return Ok((*i).release());
            else
                return i.as_type<std::unique_ptr<BencodeObject>>();
        }
        case 'd': {
            auto d = BencodeMap::parse(str, idx_inout);
            if (d)
                return Ok((*d).release());
            else
                return d.as_type<std::unique_ptr<BencodeObject>>();
        }
        case 'l': {
            auto l = BencodeList::parse(str, idx_inout);
            if (l)
                return Ok((*l).release());
            else
                return l.as_type<std::unique_ptr<BencodeObject>>();
        }
        default: {
            if (str[*idx_inout] < '0' || str[*idx_inout] > '9') {
                return Err(
                    fmt::format("invalid object type {} at {}", str[*idx_inout], *idx_inout));
            }
            auto s = BencodeString::parse(str, idx_inout);
            if (s)
                return Ok((*s).release());
            else
                return s.as_type<std::unique_ptr<BencodeObject>>();
        }
    }
}

Result<std::unique_ptr<BencodeInteger>> BencodeInteger::parse(const std::string& str,
                                                              size_t* idx_inout) {
    const auto Err = [](std::string str) -> auto {
        return Result<std::unique_ptr<BencodeInteger>>::Err(str);
    };
    const auto Ok = [](int64_t n) -> auto {
        return Result<std::unique_ptr<BencodeInteger>>::Ok(std::make_unique<BencodeInteger>(n));
    };

    auto epos = str.find('e', *idx_inout);
    if (epos == std::string::npos) {
        return Err(fmt::format("no ending mark found for integer at: {}", *idx_inout));
    } else {
        size_t len = epos - *idx_inout - 1;
        const auto n_str = str.substr(*idx_inout + 1, len);
        char* ptr;
        errno = 0;
        int64_t n = strtoll(n_str.c_str(), &ptr, 10);
        if (*ptr != '\0')
            return Err(fmt::format("expecting integer but something else received: {} at {}", n_str,
                                   *idx_inout));
        if (errno == ERANGE)
            return Err(fmt::format("integer value out of range: {} at {}", n_str, *idx_inout));
        *idx_inout = epos + 1;
        return Ok(n);
    }
}

Result<std::unique_ptr<BencodeString>> BencodeString::parse(const std::string& str,
                                                            size_t* idx_inout) {
    const auto Err = [](std::string str) -> auto {
        return Result<std::unique_ptr<BencodeString>>::Err(str);
    };
    const auto Ok = [](std::string str) -> auto {
        return Result<std::unique_ptr<BencodeString>>::Ok(std::make_unique<BencodeString>(str));
    };

    auto epos = str.find(':', *idx_inout);
    if (epos == std::string::npos)
        return Err(fmt::format("cannot find `:` mark for string at: {}", *idx_inout));

    std::string len_s = str.substr(*idx_inout, epos - *idx_inout);
    errno = 0;
    char* ptr;
    int64_t len = strtoll(len_s.c_str(), &ptr, 10);
    if (*ptr != '\0')
        return Err(fmt::format("cannot parse string length: {} at {}", len_s, *idx_inout));
    if (errno == ERANGE || len < 0)
        return Err(fmt::format("string length out of range: {} at {}", len_s, *idx_inout));

    std::string value = str.substr(epos + 1, len);
    if (value.size() != static_cast<uint64_t>(len))
        return Err(fmt::format("string ends prematurely at {}, expecting {}, has {}: {}",
                               *idx_inout, len, value.size(), value));

    *idx_inout = epos + len + 1;
    return Ok(value);
}

Result<std::unique_ptr<BencodeList>> BencodeList::parse(const std::string& str, size_t* idx_inout) {
    const auto Err = [](std::string str) -> auto {
        return Result<std::unique_ptr<BencodeList>>::Err(str);
    };

    size_t start_idx = *idx_inout;
    auto ret = std::make_unique<BencodeList>();
    ++*idx_inout;
    while (true) {
        if (*idx_inout >= str.size())
            return Err(fmt::format("list at {} ends prematurely", start_idx));
        if (str[*idx_inout] == 'e') {
            ++*idx_inout;
            break;
        }
        auto next = BencodeObject::parse(str, idx_inout);
        if (next) {
            ret->append(std::move(*next));
        } else {
            return next.as_type<std::unique_ptr<BencodeList>>();
        }
    }
    return Result<std::unique_ptr<BencodeList>>::Ok(std::move(ret));
}

Result<std::unique_ptr<BencodeMap>> BencodeMap::parse(const std::string& str, size_t* idx_inout) {
    const auto Err = [](std::string str) -> auto {
        return Result<std::unique_ptr<BencodeMap>>::Err(str);
    };

    size_t start_idx = *idx_inout;
    auto ret = std::make_unique<BencodeMap>();
    ++*idx_inout;
    while (true) {
        if (*idx_inout >= str.size())
            return Err(fmt::format("map at {} ends prematurely", start_idx));
        if (str[*idx_inout] == 'e') {
            ++*idx_inout;
            break;
        } else if ('0' <= str[*idx_inout] && str[*idx_inout] <= '9') {
            auto key = BencodeString::parse(str, idx_inout);
            if (!key) return key.as_type<std::unique_ptr<BencodeMap>>();
            if (*idx_inout >= str.size())
                return Err(fmt::format("map at {} ends prematurely", start_idx));
            auto value = BencodeObject::parse(str, idx_inout);
            if (!value) return value.as_type<std::unique_ptr<BencodeMap>>();

            std::string key_str = (*key)->value();
            auto replace = ret->set(key_str, std::move(*value));
            if (replace.get() != nullptr)
                return Err(fmt::format("duplicated key {} in map at {}", key_str, start_idx));
        } else {
            return Err(
                fmt::format("map at {} requires a string-type key at {}", start_idx, *idx_inout));
        }
    }
    return Result<std::unique_ptr<BencodeMap>>::Ok(std::move(ret));
}
}  // namespace bencode
}  // namespace ryu