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

#include "fmt/format.h"
#include "spdlog/spdlog.h"

namespace ryu {
namespace bencode {
enum class Type {
    UNSPECIFIED,
    STRING,
    INTEGER,
    LIST,
    MAP,
};

class BencodeObject;

template <typename T>
class Result {
  public:
    static Result<T> Ok(T t) { return {std::move(t)}; }
    static Result<T> Err(std::string err) { return {err, 0}; }

    Result(T t) : ok_(true), t_(std::move(t)) {}
    Result(std::string err_msg, [[maybe_unused]] int _) : ok_(false), err_(err_msg) {}

    operator bool() const { return ok_; }
    T& operator*() { return t_; }
    template <typename T2>
    Result<T2> as_type() const {
        if (ok_) {
            return Result<T2>::Err(fmt::format(
                "failed to convert error result from type {} to {}, because it's not an error",
                typeid(T).name(), typeid(T2).name()));
        } else {
            return Result<T2>::Err(err_);
        }
    }
    const std::string& err() { return err_; }

  private:
    const bool ok_;
    T t_;
    const std::string err_;
};

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

class BencodeList;
class BencodeMap;

class BencodeObject {
  public:
    static Result<std::unique_ptr<BencodeObject>> parse(const std::string& str, size_t* idx_inout);

    BencodeObject() = delete;
    virtual ~BencodeObject() = default;
    BencodeObject(const BencodeObject&) = delete;
    BencodeObject& operator=(const BencodeObject&) = delete;

    // quick accessors
    virtual std::optional<int64_t> as_int() const {return {};}
    virtual std::optional<std::string> as_string() const {return {};}
    virtual std::optional<size_t> size() const {return{};}
    template <typename T>  // T can only be uint64_t or std::string
    std::optional<T> get(size_t index) const;
    template <typename T>  // T can only be uint64_t or std::string
    std::optional<T> get(const std::string& key) const;

    virtual EncodeResult encode() const;

  protected:
    explicit BencodeObject(Type type) : type_(type) {}

  private:
    const Type type_;
};

class BencodeInteger : public BencodeObject {
  public:
    static Result<std::unique_ptr<BencodeInteger>> parse(const std::string& str, size_t* idx_inout);
    explicit BencodeInteger(int64_t val) : BencodeObject(Type::INTEGER), val_(val) {}

    int64_t value() const { return val_; }

    std::optional<int64_t> as_int() const override {return value();}
    EncodeResult encode() const override;

  private:
    int64_t val_;
};

class BencodeString : public BencodeObject {
  public:
    static Result<std::unique_ptr<BencodeString>> parse(const std::string& str, size_t* idx_inout);
    explicit BencodeString(std::string val) : BencodeObject(Type::STRING), val_(val) {}
    const std::string& value() const { return val_; };
    std::optional<std::string> as_string() const override {return value();}
    std::optional<size_t> size() const override {return val_.size();}
    EncodeResult encode() const override;
    static EncodeResult encode(const std::string& str);

  private:
    std::string val_;
};

class BencodeList : public BencodeObject {
  public:
    static Result<std::unique_ptr<BencodeList>> parse(const std::string& str, size_t* idx_inout);

    BencodeList() : BencodeObject(Type::LIST) {}
    const BencodeObject* get(size_t index) const {
        return list_.size() > index ? list_[index].get() : nullptr;
    }
    BencodeObject* get(size_t index) { return list_.size() < index ? list_[index].get() : nullptr; }
    void append(std::unique_ptr<BencodeObject> obj) { list_.push_back(std::move(obj)); }
    std::optional<size_t> size() const override {return list_.size();}
    EncodeResult encode() const override;

  private:
    std::vector<std::unique_ptr<BencodeObject>> list_;
};

class BencodeMap : public BencodeObject {
  public:
    static Result<std::unique_ptr<BencodeMap>> parse(const std::string& str, size_t* idx_inout);

    BencodeMap() : BencodeObject(Type::MAP) {}
    BencodeObject* get(const std::string& key) {
        auto val = map_.find(key);
        return val != map_.end() ? val->second.get() : nullptr;
    }
    const BencodeObject* get(const std::string& key) const {
        auto val = map_.find(key);
        return val != map_.end() ? val->second.get() : nullptr;
    }
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
    std::optional<size_t> size() const override {return map_.size();}
    EncodeResult encode() const override;

  private:
    std::map<std::string, std::unique_ptr<BencodeObject>> map_;
};

}  // namespace bencode
}  // namespace ryu
