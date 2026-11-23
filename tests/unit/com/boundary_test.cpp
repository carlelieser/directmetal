#include <directmetal/com/boundary.hpp>

#include <gtest/gtest.h>

#include <new>
#include <stdexcept>

namespace {

using directmetal::com::guard;

TEST(BoundaryTest, ReturnsTheBodysResult) {
    EXPECT_EQ(guard("Test::Succeeds", [] { return S_FALSE; }), S_FALSE);
    EXPECT_EQ(guard("Test::Fails", [] { return E_INVALIDARG; }), E_INVALIDARG);
}

TEST(BoundaryTest, AllocationFailureIsOutOfMemory) {
    EXPECT_EQ(guard("Test::Allocates", []() -> HRESULT { throw std::bad_alloc(); }), E_OUTOFMEMORY);
}

TEST(BoundaryTest, OtherExceptionsTerminate) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(guard("Test::Throws", []() -> HRESULT { throw std::logic_error("bug"); }), "");
}

} // namespace
