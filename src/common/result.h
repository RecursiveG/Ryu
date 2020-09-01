#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/strings/string_view.h"

namespace ryu {

struct UnTypedError {
    std::string msg;
};
inline UnTypedError Err(std::string_view msg) { return {std::string(msg)}; }

template <typename T>
class Result {
    static_assert(std::is_default_constructible<T>::value,
                  "For Result<T>, T is required to be default constructable.");

  public:
    Result(T t) : ok_(true), t_(std::move(t)) {}
    template <typename R>
    // special convertion from Result<unique_ptr<Derived>> to
    // Result<unique_ptr<Base>>
    Result(Result<std::unique_ptr<R>> r)
        : ok_(r.ok_),
          t_(static_cast<typename T::element_type*>(r.t_.release())),
          err_(std::move(r.err_)) {}
    Result(UnTypedError err) : ok_(false), err_(std::move(err.msg)) {}
    Result(Result&& another) noexcept
        : ok_(another.ok_), t_(std::move(another.t_)), err_(std::move(another.err_)) {}
    Result& operator=(Result&& another) noexcept {
        if (this == &another) return *this;
        ok_ = another.ok_;
        std::swap(t_, another.t_);
        std::swap(err_, another.err_);
        another.ok_ = false;
        another.err_ = "Content of this object has been moved";
        return *this;
    };

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // bool() operators
    [[nodiscard]] bool Ok() const { return ok_; }
    [[nodiscard]] explicit operator bool() const { return ok_; }

    // Value accessors
    T& Value() {
        if (ABSL_PREDICT_FALSE(!Ok())) throw std::logic_error("taking value of an error result");
        return t_;
    }
    const T& Value() const {
        if (ABSL_PREDICT_FALSE(!Ok())) throw std::logic_error("taking value of an error result");
        return t_;
    }
    T Take() && {
        if (ABSL_PREDICT_FALSE(!Ok())) throw std::logic_error("taking value of an error result");
        return std::move(t_);
    }
    T TakeOrRaise() && {
        if (!Ok()) throw std::logic_error(err_);
        return std::move(t_);
    }

    // Error accessors
    [[nodiscard]] const std::string& Err() const {
        if (ABSL_PREDICT_FALSE(Ok()))
            throw std::logic_error("taking error message of an Ok result");
        return err_;
    }
    [[nodiscard]] UnTypedError TakeErr() && {
        if (ABSL_PREDICT_FALSE(Ok()))
            throw std::logic_error("taking error message of an Ok result");
        return {std::move(err_)};
    };

    // transformations
    template <typename F, typename R = std::result_of_t<F(T)>>
    [[nodiscard]] Result<R> fmap(const F& f) && {
        if (Ok()) {
            return f(std::move(t_));
        } else {
            return std::move(*this).TakeErr();
        }
    }
    template <typename F, typename R = std::result_of_t<F(T)>>
    // F must also return a Result<...>
    [[nodiscard]] R bind(const F& f) && {
        if (Ok()) {
            return f(std::move(t_));
        } else {
            return std::move(*this).TakeErr();
        }
    }

  private:
    template <typename AnotherT>
    friend class Result;
    bool ok_;
    T t_;
    std::string err_;
};

#define ASSIGN_OR_RAISE(var, result_expr)        \
    var = ({                                     \
        auto result_ = (result_expr);            \
        if (!result_.Ok()) {                     \
            return std::move(result_).TakeErr(); \
        }                                        \
        std::move(result_).Take();               \
    })

}  // namespace ryu