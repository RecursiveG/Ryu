#pragma once

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
    // template<typename R, typename = typename std::enable_if<std::is_same<T, R>::value>::type>
    Result(T t) : ok_(true), t_(std::move(t)) {}
    template <typename R>  // special convertion from Result<unique_ptr<Derived>> to
                           // Result<unique_ptr<Base>>
    Result(Result<std::unique_ptr<R>> r)
        : ok_(r.ok_),
          t_(static_cast<typename T::element_type*>(r.t_.release())),
          err_(std::move(r.err_)) {}
    Result(UnTypedError err) : ok_(false), err_(std::move(err.msg)) {}
    Result(Result&& another) noexcept
        : ok_(another.ok_), t_(std::move(another.t_)), err_(std::move(another.err_)) {}

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result& operator=(Result&&) = delete;

    [[nodiscard]] bool Ok() const { return ok_; }
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

    //    template <typename T2>
    //    Result<T2> as_type() const {
    //        if (ok_) {
    //            return Result<T2>::Err(fmt::format(
    //                "failed to convert error result from type {} to {}, because it's not an
    //                error", typeid(T).name(), typeid(T2).name()));
    //        } else {
    //            return Result<T2>::Err(err_);
    //        }
    //    }
    [[nodiscard]] const std::string& Err() const {
        if (ABSL_PREDICT_FALSE(Ok()))
            throw std::logic_error("taking error message of an Ok result");
        return err_;
    }
    [[nodiscard]] UnTypedError UntypedErr() && {
        if (ABSL_PREDICT_FALSE(Ok()))
            throw std::logic_error("taking error message of an Ok result");
        return {err_};
    };

  private:
    template <typename AnotherT>
    friend class Result;
    const bool ok_;
    T t_;
    const std::string err_;
};

template <typename T>
using AllocationResult = Result<std::unique_ptr<T>>;

#define ASSIGN_OR_RAISE(var, result_expr)           \
    var = ({                                        \
        auto result_ = (result_expr);               \
        if (!result_.Ok()) {                        \
            return std::move(result_).UntypedErr(); \
        }                                           \
        std::move(result_).Take();                  \
    })

}  // namespace ryu