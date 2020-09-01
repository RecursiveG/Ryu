#include "result.h"

#include <gtest/gtest.h>

#include <cinttypes>
#include <memory>
#include <stdexcept>
#include <string>
template <typename T>
using AllocationResult = ryu::Result<std::unique_ptr<T>>;
using ryu::Err;
using ryu::Result;

Result<int> ReturnCopyable() { return 42; }
TEST(ResultTest, ReturnVal) {
    auto r = ReturnCopyable();
    EXPECT_TRUE(r.Ok());
    EXPECT_EQ(r.Value(), 42);
}

Result<int> ReturnError() { return Err("foo"); }
TEST(ResultTest, ReturnErr) {
    auto r = ReturnError();
    EXPECT_FALSE(r.Ok());
    EXPECT_EQ(r.Err(), "foo");
}

Result<std::string> ChangeErrType(Result<int> r) {
    ASSIGN_OR_RAISE(auto x [[maybe_unused]], std::move(r));
    return std::string{};
};
TEST(ResultTest, ChangeErrorType) {
    auto x = ChangeErrType(Err("bar"));
    EXPECT_FALSE(x.Ok());
    EXPECT_EQ(x.Err(), "bar");
}

Result<int> PlusOne(Result<int> in) {
    ASSIGN_OR_RAISE(int x, std::move(in));
    return x + 1;
}
TEST(ResultTest, PlusOne) {
    auto x = PlusOne(5);
    EXPECT_TRUE(x.Ok());
    EXPECT_EQ(x.Value(), 6);
    auto y = PlusOne(Err("boo"));
    EXPECT_FALSE(y.Ok());
    EXPECT_EQ(y.Err(), "boo");
}

AllocationResult<uint8_t[]> AllocateBuffer(size_t size) {
    return std::make_unique<uint8_t[]>(size);
}
TEST(ResultTest, ReturnUniquePtr) {
    auto x = AllocateBuffer(4096).Take();
    static_assert(std::is_same<decltype(x), std::unique_ptr<uint8_t[]>>::value);
}

class CopyBomb {
  public:
    CopyBomb() : val_(0){};
    CopyBomb(int val) : val_(val){};
    CopyBomb(const CopyBomb&) : val_(0) {
        throw std::logic_error("copy constructor bomb triggered");
    }
    CopyBomb& operator=(const CopyBomb&) {
        throw std::logic_error("copy assignment bomb triggered");
    }
    CopyBomb(CopyBomb&&) = default;
    CopyBomb& operator=(CopyBomb&&) = default;
    int val_;
};
Result<CopyBomb> CreateBomb() { return {42}; }
Result<CopyBomb> BombPlusOne() {
    ASSIGN_OR_RAISE(auto b, CreateBomb());
    b.val_++;
    return b;
}
TEST(ResultTest, NoExtraCopy) {
    auto x = BombPlusOne();
    EXPECT_TRUE(x.Ok());
    EXPECT_EQ(x.Value().val_, 43);
}

struct Base {
    virtual ~Base() = default;
};
struct Derived : public Base {
    Derived(int x) : x_(x){};
    int x_;
};
AllocationResult<Derived> AllocDerived(int x) { return std::make_unique<Derived>(x); };
AllocationResult<Base> ReturnPointerCovariant(AllocationResult<Derived> in) { return in; };
TEST(ResultTest, PointerCovariant) {
    auto x = ReturnPointerCovariant(AllocDerived(42));
    EXPECT_TRUE(x.Ok());
    auto d_ptr = dynamic_cast<const Derived*>(x.Value().get());
    ASSERT_NE(d_ptr, nullptr);
    EXPECT_EQ(d_ptr->x_, 42);
    auto y = ReturnPointerCovariant(Err("bbbb"));
    EXPECT_FALSE(y.Ok());
    EXPECT_EQ(y.Err(), "bbbb");
}

AllocationResult<Base> ReturnPointer(AllocationResult<Base> b) { return b; }
TEST(ResultTest, SmartPointer) {
    auto x = ReturnPointer(std::make_unique<Base>());
    EXPECT_TRUE(x.Ok());
    ASSERT_EQ(dynamic_cast<const Derived*>(x.Value().get()), nullptr);
}

TEST(ResultTest, ResultFmap) {
    bool called = false;
    Result<int> x = 0;
    auto plus1 = [&](int v) -> int {
        called = true;
        return v + 1;
    };

    x = Result<int>{Err("bbz")}.fmap(plus1);
    EXPECT_FALSE(x);
    EXPECT_EQ(x.Err(), "bbz");
    EXPECT_FALSE(called);

    x = Result<int>{42}.fmap(plus1);
    EXPECT_TRUE(x);
    EXPECT_EQ(x.Value(), 43);
    EXPECT_TRUE(called);
}

TEST(ResultTest, ResultBind) {
    bool good_called = false;
    bool bad_called = false;
    auto good = [&](int x) {
        good_called = true;
        return Result<int>{x + 1};
    };
    auto bad = [&](int _ [[maybe_unused]]) -> Result<int> {
        bad_called = true;
        return Err("bad");
    };

    Result<int> x = 42;
    x = std::move(x).bind(good);
    EXPECT_TRUE(x);
    EXPECT_EQ(x.Value(), 43);
    EXPECT_TRUE(good_called);

    good_called = false;
    x = Result<int>{Err("fail")}.bind(good);
    EXPECT_FALSE(x);
    EXPECT_EQ(x.Err(), "fail");
    EXPECT_FALSE(good_called);

    x = Result<int>{42}.bind(bad);
    EXPECT_FALSE(x);
    EXPECT_EQ(x.Err(), "bad");
    EXPECT_TRUE(bad_called);

    bad_called = false;
    x = Result<int>{Err("fail")}.bind(bad);
    EXPECT_FALSE(x);
    EXPECT_EQ(x.Err(), "fail");
    EXPECT_FALSE(bad_called);
}
