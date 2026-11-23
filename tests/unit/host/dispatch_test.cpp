#include <directmetal/host/dispatch.hpp>

#include <gtest/gtest.h>

namespace {

using namespace directmetal::bridge;
using directmetal::host::dispatch;

TEST(Dispatch, AnswersQueryVersion) {
    QueryVersionParams params{};
    EXPECT_EQ(dispatch(to_index(FunctionCode::query_version), &params), status_success);
    EXPECT_EQ(params.version, protocol_version);
}

TEST(Dispatch, ReportsUnimplementedControlCalls) {
    for (auto code = to_index(FunctionCode::initialize); code < to_index(FunctionCode::count);
         ++code) {
        EXPECT_EQ(dispatch(code, nullptr), status_not_implemented) << "code " << code;
    }
}

TEST(Dispatch, RejectsUnknownCodes) {
    EXPECT_EQ(dispatch(to_index(FunctionCode::count), nullptr), status_invalid_parameter);
    EXPECT_EQ(dispatch(0xFFFFFFFF, nullptr), status_invalid_parameter);
}

} // namespace
