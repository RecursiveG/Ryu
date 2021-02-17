#pragma once

#include <cinttypes>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <result.h>

#include "ordered_map.h"

namespace ryu {
namespace bencode {

template <typename T>
using PointerResult = ::Result<std::unique_ptr<T>, std::string>;
using EncodeResult = ::Result<std::string, std::string>;
enum class Type {
    Invalid,
    Integer,
    String,
    List,
    Map,
};

class BencodeObject;
class BencodeInteger;
class BencodeString;
class BencodeList;
class BencodeMap;

class BencodeObject {
  public:
    static BencodeObject* invalid() {
        static BencodeObject invalid_obj;
        return &invalid_obj;
    }

    virtual ~BencodeObject() = default;
    BencodeObject(const BencodeObject&) = delete;
    BencodeObject& operator=(const BencodeObject&) = delete;

    virtual Type GetType() const { return Type::Invalid; }
    // int only
    virtual std::optional<int64_t> GetInt() const { return {}; }
    virtual bool Set(int64_t val) { return false; }
    // string only
    virtual std::optional<std::string> GetString() const { return {}; }
    virtual bool Set(const std::string& str) { return false; }
    // list only
    virtual const BencodeObject& operator[](size_t index) const { return *invalid(); }
    virtual bool Add(std::unique_ptr<BencodeObject> obj) { return false; }
    virtual bool Set(size_t index, std::unique_ptr<BencodeObject> obj) { return false; }
    virtual std::unique_ptr<BencodeObject> Del(size_t index) { return nullptr; }
    // map only
    virtual const BencodeObject& operator[](const std::string& key) const { return *invalid(); }
    virtual bool Set(const std::string& key, std::unique_ptr<BencodeObject> obj) { return false; }
    virtual std::unique_ptr<BencodeObject> Del(const std::string& key) { return nullptr; }
    virtual bool Contains(const std::string& key) const { return false; }
    // both list and map
    virtual size_t Size() const { return -1; }

    // parser and serializer
    // BencodeObject::parse() determines the type of next object
    // Subclass::parse() assume the object type is correct
    static PointerResult<BencodeObject> Parse(const std::string& str, size_t* idx_inout);
    // convert this object to bencode format
    virtual EncodeResult Encode() const {
        return Err("Cannot encode invalid object");
    }
    // convert this object to json format
    virtual EncodeResult Json() const {
        return Err("Cannot encode invalid object");
    }

  protected:
    BencodeObject() = default;
};

class BencodeInteger : public BencodeObject {
  public:
    Type GetType() const override { return Type::Integer; }
    // int only
    std::optional<int64_t> GetInt() const override { return val_; }
    bool Set(int64_t val) override { val_ = val; return true; }

    // constructor, parser, serializer
    explicit BencodeInteger(int64_t val) : val_(val) {}
    static PointerResult<BencodeInteger> Parse(const std::string& str, size_t* idx_inout);
    EncodeResult Encode() const override;
    EncodeResult Json() const override;

  private:
    int64_t val_;
};

class BencodeString : public BencodeObject {
  public:
    Type GetType() const override { return Type::String; }
    // string only
    std::optional<std::string> GetString() const override { return val_; }
    bool Set(const std::string& str) override { val_ = str; return true; }

    // constructor, parser, serializer
    explicit BencodeString(const std::string& val) : val_(val) {}
    static PointerResult<BencodeString> Parse(const std::string& str, size_t* idx_inout);
    EncodeResult Encode() const override;
    EncodeResult Json() const override;

    // directly encode, same as BencodeString(str).Encode()
    static EncodeResult Encode(const std::string& str);

  private:
    std::string val_;
};

class BencodeList : public BencodeObject {
  public:
    Type GetType() const override { return Type::List; }
    // list only
    const BencodeObject& operator[](size_t index) const override {
        return *list_.at(index).get();
    }
    bool Add(std::unique_ptr<BencodeObject> obj) override { 
        list_.push_back(std::move(obj));
        return true;
    }
    bool Set(size_t index, std::unique_ptr<BencodeObject> obj) override {
        list_.at(index) = std::move(obj);
        return true;
    }
    std::unique_ptr<BencodeObject> Del(size_t index) override {
        if (index >= list_.size()) return nullptr;
        auto ret = std::move(list_.at(index));
        list_.erase(list_.begin() + index);
        return ret;
    }
    // both list and map
    size_t Size() const override {
        return list_.size();
    }

    // constructor, parser, serializer
    BencodeList() = default;
    static PointerResult<BencodeList> Parse(const std::string& str, size_t* idx_inout);
    EncodeResult Encode() const override;
    EncodeResult Json() const override;

  private:
    std::vector<std::unique_ptr<BencodeObject>> list_;
};

class BencodeMap : public BencodeObject {
  public:
    Type GetType() const override { return Type::Map; }
    // map only
    const BencodeObject& operator[](const std::string& key) const override {
        return *map_[key].get();
    }
    bool Set(const std::string& key, std::unique_ptr<BencodeObject> obj) override {
        map_.insert(key, std::move(obj));
        return true;
    }
    std::unique_ptr<BencodeObject> Del(const std::string& key) override {
        auto ret = map_.erase(key);
        return ret ? std::move(*ret) : nullptr;
    }
    bool Contains(const std::string& key) const override {
        return map_.contains(key);
    }
    // both list and map
    virtual size_t Size() const override { return map_.size(); }

    // constructor, parser, serializer
    BencodeMap() = default;
    static PointerResult<BencodeMap> Parse(const std::string& str, size_t* idx_inout);
    EncodeResult Encode() const override;
    EncodeResult Json() const override;

  private:
    ordered_map<std::string, std::unique_ptr<BencodeObject>> map_;
};

}  // namespace bencode
}  // namespace ryu
