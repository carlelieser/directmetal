#include <directmetal/bridge/control.hpp>
#include <directmetal/unixlib.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>

// The process-wide host behind both Wine call tables. Nothing here creates a Metal device: these
// tests run in suite unit, so every call fails before it reaches the backend.
namespace {

using namespace directmetal::bridge;

constexpr std::size_t code_count = to_index(FunctionCode::count);

Status call(const directmetal::UnixCallEntry* table, FunctionCode code, void* params) {
    return table[to_index(code)](params);
}

void initialize_native() {
    InitializeParams params{0, 0, 0, 0, 0};
    ASSERT_EQ(call(__wine_unix_call_funcs, FunctionCode::initialize, &params), status_success);
}

// Parameters that each code answers in its own way once the host is initialized.
struct Probe {
    alignas(8) std::array<std::byte, 256> bytes{};
};

Probe probe_for(FunctionCode code) {
    Probe probe;
    if (code == FunctionCode::enumerate_adapters) {
        const EnumerateAdaptersParams params{0, 1, {}};
        std::memcpy(probe.bytes.data(), &params, sizeof(params));
    }
    if (code == FunctionCode::create_device) {
        const CreateDeviceParams params{0, {1, 0}, {}, {}};
        std::memcpy(probe.bytes.data(), &params, sizeof(params));
    }
    return probe;
}

// Indexed by FunctionCode: the host's reply to probe_for(code).
constexpr std::array<Status, code_count> expected_replies = {
    status_success,           // query_version
    status_invalid_parameter, // log_call: no name
    status_success,           // initialize
    status_invalid_parameter, // enumerate_adapters: reserved field set
    status_invalid_parameter, // create_device: heap block size not 16 KiB aligned
    status_invalid_handle,    // destroy_device
    status_invalid_handle,    // create_buffer
    status_invalid_handle,    // import_memory
    status_invalid_handle,    // create_texture
    status_invalid_handle,    // create_timeline
    status_invalid_handle,    // open_stream
    status_invalid_handle,    // submit_stream
    status_invalid_handle,    // wait_timeline
    status_invalid_handle,    // close_stream
    status_invalid_handle,    // release_memory
    status_invalid_handle,    // destroy_timeline
};

TEST(UnixCallTables, NativeTableRoutesEveryCodeToItsHandler) {
    initialize_native();
    for (std::size_t index = 0; index < code_count; ++index) {
        const auto code = static_cast<FunctionCode>(index);
        Probe probe = probe_for(code);
        EXPECT_EQ(call(__wine_unix_call_funcs, code, probe.bytes.data()), expected_replies[index])
            << "code " << index;
    }
}

TEST(UnixCallTables, Wow64TableIdentifiesItsCallers) {
    initialize_native();
    InitializeParams claims_native{0, 0, 0, 0, 0};
    EXPECT_EQ(call(__wine_unix_call_wow64_funcs, FunctionCode::initialize, &claims_native),
              status_invalid_parameter);
    InitializeParams claims_wow64{0, 0, initialize_flag_wow64, 0, 0};
    EXPECT_EQ(call(__wine_unix_call_funcs, FunctionCode::initialize, &claims_wow64),
              status_invalid_parameter);
    // The host was initialized through the native table, so WoW64 callers are refused.
    WaitTimelineParams wait{{1}, 0, 0};
    EXPECT_EQ(call(__wine_unix_call_wow64_funcs, FunctionCode::wait_timeline, &wait),
              status_invalid_parameter);
    QueryVersionParams version{};
    EXPECT_EQ(call(__wine_unix_call_wow64_funcs, FunctionCode::query_version, &version),
              status_success);
    EXPECT_EQ(version.version, protocol_version);
}

TEST(UnixCallTables, RejectNullParameters) {
    for (std::size_t index = 0; index < code_count; ++index) {
        EXPECT_EQ(__wine_unix_call_funcs[index](nullptr), status_invalid_parameter) << index;
        EXPECT_EQ(__wine_unix_call_wow64_funcs[index](nullptr), status_invalid_parameter) << index;
    }
}

} // namespace
