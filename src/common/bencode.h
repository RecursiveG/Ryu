#pragma once

#include <cinttypes>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "result.h"

namespace ryu {
namespace bencode {

class BencodeObject;

class EncodeResult {
  public:
    static EncodeResult Ok(std::string bencode) { return {true, bencode}; }
    static EncodeResult Err(std::string msg) { return {false, msg}; }
    operator bool() const { return success_; }

    const std::string& operator*() const { return str_; }
    void concat(const std::function<const EncodeResult()>& next) {
        if (success_) {
            const auto n = next();
            if (n) {
                str_ += *n;
            } else {
                success_ = false;
                str_ = *n;
            }
        }
    }

  private:
    EncodeResult(bool success, std::string str) : success_(success), str_(str) {}
    bool success_;
    std::string str_;
};

class BencodeInteger;
class BencodeString;
class BencodeList;
class BencodeMap;

// Abstruct base class
class BencodeObject {
  public:
    virtual ~BencodeObject() = default;
    BencodeObject(const BencodeObject&) = delete;
    BencodeObject& operator=(const BencodeObject&) = delete;

    // type castings, may return null
    // TODO need const variants
    virtual BencodeInteger* try_int_object() { return nullptr; }
    virtual BencodeString* try_string_object() { return nullptr; }
    virtual BencodeList* try_list_object() { return nullptr; }
    virtual BencodeMap* try_map_object() { return nullptr; }

    // quick accessors
    virtual std::optional<int64_t> try_int() const { return {}; }
    virtual std::optional<std::string> try_string() const { return {}; }
    virtual std::optional<size_t> try_size() const { return {}; }

    virtual std::optional<int64_t> try_int([[maybe_unused]] size_t idx) const { return {}; }
    virtual std::optional<std::string> try_string([[maybe_unused]] size_t idx) const { return {}; }
    virtual BencodeObject* try_object([[maybe_unused]] size_t idx) const { return {}; }

    virtual std::optional<int64_t> try_int([[maybe_unused]] const std::string& key) const {
        return {};
    }
    virtual std::optional<std::string> try_string([[maybe_unused]] const std::string& key) const {
        return {};
    }
    virtual BencodeObject* try_object([[maybe_unused]] const std::string& key) const { return {}; }

    // parser and serializer
    static Result<std::unique_ptr<BencodeObject>> parse(const std::string& str, size_t* idx_inout);
    // convert this object to bencode format
    virtual EncodeResult encode() const = 0;
    // convert this object to json format
    virtual EncodeResult json() const = 0;

  protected:
    BencodeObject() = default;
};

class BencodeInteger : public BencodeObject {
  public:
    // accessors
    BencodeInteger* try_int_object() override { return this; }
    std::optional<int64_t> try_int() const override { return val_; }
    int64_t value() const { return val_; }

    // constructor, parser, serializer
    explicit BencodeInteger(int64_t val) : BencodeObject(), val_(val) {}
    static Result<std::unique_ptr<BencodeInteger>> parse(const std::string& str, size_t* idx_inout);
    EncodeResult encode() const override;
    EncodeResult json() const override;

  private:
    int64_t val_;
};

class BencodeString : public BencodeObject {
  public:
    // accessors
    BencodeString* try_string_object() override { return this; }
    std::optional<std::string> try_string() const override { return val_; }
    std::string value() const { return val_; }

    // constructor, parser, serializer
    explicit BencodeString(std::string val) : BencodeObject(), val_(val) {}
    static Result<std::unique_ptr<BencodeString>> parse(const std::string& str, size_t* idx_inout);
    EncodeResult encode() const override;
    EncodeResult json() const override;

    // directly encode
    static EncodeResult encode(const std::string& str);

  private:
    std::string val_;
};

class BencodeList : public BencodeObject {
  public:
    // accessors
    BencodeList* try_list_object() override { return this; }
    std::optional<size_t> try_size() const override { return list_.size(); }
    std::optional<int64_t> try_int(size_t idx) const override {
        if (idx < list_.size())
            return list_[idx]->try_int();
        else
            return {};
    }
    std::optional<std::string> try_string(size_t idx) const override {
        if (idx < list_.size())
            return list_[idx]->try_string();
        else
            return {};
    }
    BencodeObject* try_object(size_t idx) const override {
        if (idx < list_.size())
            return list_[idx].get();
        else
            return nullptr;
    }

    // constructor, parser, serializer
    BencodeList() = default;
    static Result<std::unique_ptr<BencodeList>> parse(const std::string& str, size_t* idx_inout);
    EncodeResult encode() const override;
    EncodeResult json() const override;

    // modifiers
    void append(std::unique_ptr<BencodeObject> obj) { list_.push_back(std::move(obj)); }

  private:
    std::vector<std::unique_ptr<BencodeObject>> list_;
};

class BencodeMap : public BencodeObject {
  public:
    // accessors
    BencodeMap* try_map_object() override { return this; }
    std::optional<size_t> try_size() const override { return map_.size(); }
    std::optional<int64_t> try_int(const std::string& key) const override {
        const auto& it = map_.find(key);
        if (it != map_.end()) {
            return it->second->try_int();
        } else {
            return {};
        }
    }
    std::optional<std::string> try_string(const std::string& key) const override {
        const auto& it = map_.find(key);
        if (it != map_.end()) {
            return it->second->try_string();
        } else {
            return {};
        }
    }
    BencodeObject* try_object(const std::string& key) const override {
        const auto& it = map_.find(key);
        if (it != map_.end()) {
            return it->second.get();
        } else {
            return nullptr;
        }
    }

    // constructor, parser, serializer
    BencodeMap() = default;
    static Result<std::unique_ptr<BencodeMap>> parse(const std::string& str, size_t* idx_inout);
    EncodeResult encode() const override;
    EncodeResult json() const override;

    // modifier, return the old value if any
    std::unique_ptr<BencodeObject> set(const std::string& key, std::unique_ptr<BencodeObject> val) {
        const auto& it = map_.find(key);
        if (it == map_.end()) {
            map_.emplace(key, std::move(val));
            return {};
        } else {
            std::swap(val, it->second);
            return val;
        }
    }

  private:
    std::map<std::string, std::unique_ptr<BencodeObject>> map_;
};

}  // namespace bencode
}  // namespace ryu
