#include <directmetal/bridge/control.hpp>
#include <directmetal/unixlib.hpp>

#include <gtest/gtest.h>

#include <string>

// The process-wide host behind both Wine call tables. Nothing here creates a Metal device: these
// tests run in suite unit.
namespace {

using namespace directmetal::bridge;

struct CallTable {
    const char* name;
    const directmetal::UnixCallEntry* entries;
};

class UnixCallTable : public testing::TestWithParam<CallTable> {
protected:
    Status call(FunctionCode code, void* params) const {
        return GetParam().entries[to_index(code)](params);
    }
};

TEST_P(UnixCallTable, RoutesEveryCodeToHost) {
    for (auto code = to_index(FunctionCode::query_version); code < to_index(FunctionCode::count);
         ++code) {
        EXPECT_EQ(GetParam().entries[code](nullptr), status_invalid_parameter) << "code " << code;
    }
}

TEST_P(UnixCallTable, AnswersQueryVersion) {
    QueryVersionParams params{};
    EXPECT_EQ(call(FunctionCode::query_version, &params), status_success);
    EXPECT_EQ(params.version, protocol_version);
}

TEST_P(UnixCallTable, InitializesHost) {
    InitializeParams params{0, 0, 0, 0, 0};
    EXPECT_EQ(call(FunctionCode::initialize, &params), status_success);
    // Rejected before the adapter list, and so the Metal device, is touched.
    EnumerateAdaptersParams adapters{0, 1, {}};
    EXPECT_EQ(call(FunctionCode::enumerate_adapters, &adapters), status_invalid_parameter);
    WaitTimelineParams wait{{1}, 0, 0};
    EXPECT_EQ(call(FunctionCode::wait_timeline, &wait), status_invalid_handle);
}

INSTANTIATE_TEST_SUITE_P(Tables, UnixCallTable,
                         testing::Values(CallTable{"native", __wine_unix_call_funcs},
                                         CallTable{"wow64", __wine_unix_call_wow64_funcs}),
                         [](const auto& info) { return std::string(info.param.name); });

} // namespace
