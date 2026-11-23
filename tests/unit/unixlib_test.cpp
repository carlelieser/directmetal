#include <directmetal/unixlib.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <string_view>
#include <sys/utsname.h>

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

TEST_P(UnixCallTable, QueryVersionReportsProtocolVersion) {
    QueryVersionParams params{};
    EXPECT_EQ(call(FunctionCode::query_version, &params), status_success);
    EXPECT_EQ(params.version, protocol_version);
}

TEST_P(UnixCallTable, QueryVersionRejectsNonZeroReserved) {
    QueryVersionParams params{0, 1};
    EXPECT_EQ(call(FunctionCode::query_version, &params), status_invalid_parameter);
    EXPECT_EQ(call(FunctionCode::query_version, nullptr), status_invalid_parameter);
}

// The WoW64 table refuses 64-bit addresses, which is covered in tests/unit/host; these two run on
// the native table only.
Status native_call(FunctionCode code, void* params) {
    return __wine_unix_call_funcs[to_index(code)](params);
}

TEST(NativeCallTable, LogCallWritesNameToStderr) {
    constexpr std::string_view name = "D3D11CreateDevice";
    LogCallParams params{to_address(name.data()), static_cast<std::uint32_t>(name.size()), 0, {}};
    testing::internal::CaptureStderr();
    const Status status = native_call(FunctionCode::log_call, &params);
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "directmetal: call D3D11CreateDevice\n");
    EXPECT_EQ(status, status_success);
}

TEST(NativeCallTable, LogCallRepliesWithHostSystem) {
    constexpr std::string_view name = "D3D11CreateDevice";
    LogCallParams params{to_address(name.data()), static_cast<std::uint32_t>(name.size()), 0, {}};
    std::memset(params.host_system, 'x', sizeof(params.host_system));
    testing::internal::CaptureStderr();
    const Status status = native_call(FunctionCode::log_call, &params);
    testing::internal::GetCapturedStderr();
    utsname host{};
    ASSERT_EQ(uname(&host), 0);
    EXPECT_EQ(status, status_success);
    EXPECT_STREQ(params.host_system, host.sysname);
    EXPECT_STREQ(params.host_system, "Darwin");
}

TEST_P(UnixCallTable, LogCallRejectsMissingName) {
    LogCallParams params{0, 4, 0, {}};
    EXPECT_EQ(call(FunctionCode::log_call, &params), status_invalid_parameter);
    EXPECT_EQ(call(FunctionCode::log_call, nullptr), status_invalid_parameter);
}

TEST_P(UnixCallTable, LogCallRejectsNonZeroReserved) {
    constexpr std::string_view name = "D3D11CreateDevice";
    LogCallParams params{to_address(name.data()), static_cast<std::uint32_t>(name.size()), 1, {}};
    testing::internal::CaptureStderr();
    const Status status = call(FunctionCode::log_call, &params);
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
    EXPECT_EQ(status, status_invalid_parameter);
}

TEST_P(UnixCallTable, LogCallRejectsOverlongName) {
    const std::string name(max_name_length + 1, 'x');
    LogCallParams params{to_address(name.data()), max_name_length + 1, 0, {}};
    EXPECT_EQ(call(FunctionCode::log_call, &params), status_invalid_parameter);
}

INSTANTIATE_TEST_SUITE_P(Tables, UnixCallTable,
                         testing::Values(CallTable{"native", __wine_unix_call_funcs},
                                         CallTable{"wow64", __wine_unix_call_wow64_funcs}),
                         [](const auto& info) { return std::string(info.param.name); });

} // namespace
