#include "host_fixture.hpp"

#include <string_view>

namespace directmetal::host::testing {
namespace {

using namespace bridge;

TEST_F(HostTest, AnswersQueryVersionBeforeInitialize) {
    QueryVersionParams params{};
    EXPECT_EQ(call(FunctionCode::query_version, params), status_success);
    EXPECT_EQ(params.version, protocol_version);
}

TEST_F(HostTest, RejectsCallsBeforeInitialize) {
    for (auto code = to_index(FunctionCode::enumerate_adapters);
         code < to_index(FunctionCode::count); ++code) {
        std::uint64_t params[32] = {};
        EXPECT_EQ(host->call(code, params), status_unsuccessful) << "code " << code;
    }
}

TEST_F(HostTest, RejectsNullParametersAndUnknownCodes) {
    initialize();
    for (auto code = to_index(FunctionCode::query_version); code < to_index(FunctionCode::count);
         ++code) {
        EXPECT_EQ(host->call(code, nullptr), status_invalid_parameter) << "code " << code;
    }
    EXPECT_EQ(host->call(to_index(FunctionCode::count), nullptr), status_invalid_parameter);
    EXPECT_EQ(host->call(UINT32_MAX, nullptr), status_invalid_parameter);
}

TEST_F(HostTest, LogsCallsBeforeInitialize) {
    constexpr std::string_view name = "D3D11CreateDevice";
    LogCallParams params{
        bridge::to_address(name.data()), static_cast<std::uint32_t>(name.size()), 0, {}};
    ::testing::internal::CaptureStderr();
    const Status status = call(FunctionCode::log_call, params);
    EXPECT_EQ(::testing::internal::GetCapturedStderr(), "directmetal: call D3D11CreateDevice\n");
    EXPECT_EQ(status, status_success);
    EXPECT_STREQ(params.host_system, "Darwin");
}

TEST_F(HostTest, RepeatsInitialize) {
    initialize();
    initialize();
    EXPECT_NE(create_device().value, 0u);
}

TEST_F(HostTest, RejectsInvalidInitialize) {
    InitializeParams unknown_flag{0, 0, 0x2, 0, 0};
    EXPECT_EQ(call(FunctionCode::initialize, unknown_flag), status_invalid_parameter);
    InitializeParams reserved{0, 0, 0, 0, 1};
    EXPECT_EQ(call(FunctionCode::initialize, reserved), status_invalid_parameter);
    InitializeParams long_name{1, max_name_length + 1, 0, 0, 0};
    EXPECT_EQ(call(FunctionCode::initialize, long_name), status_invalid_parameter);
    InitializeParams log_level{0, 0, 0, 6, 0};
    EXPECT_EQ(call(FunctionCode::initialize, log_level), status_invalid_parameter);
    CreateDeviceParams device{};
    EXPECT_EQ(call(FunctionCode::create_device, device), status_unsuccessful);
}

TEST_F(HostTest, RejectsInitializeChangingWow64) {
    initialize(false);
    InitializeParams params{0, 0, initialize_flag_wow64, 0, 0};
    EXPECT_EQ(call(FunctionCode::initialize, params), status_invalid_parameter);
}

TEST_F(HostTest, EnumeratesProviderAdapters) {
    initialize();
    EnumerateAdaptersParams first{0, 0, {}};
    EXPECT_EQ(call(FunctionCode::enumerate_adapters, first), status_success);
    EXPECT_EQ(first.adapter.registry_id, 42u);
    EXPECT_STREQ(first.adapter.name.data(), "Fake");
    EnumerateAdaptersParams past{1, 0, {}};
    EXPECT_EQ(call(FunctionCode::enumerate_adapters, past), status_no_more_entries);
    EnumerateAdaptersParams reserved{0, 1, {}};
    EXPECT_EQ(call(FunctionCode::enumerate_adapters, reserved), status_invalid_parameter);
}

TEST_F(HostTest, CreatesDeviceWithRequestedOptions) {
    initialize();
    CreateDeviceParams params{7, {1u << 20, 1u << 19}, {}, {}};
    EXPECT_EQ(call(FunctionCode::create_device, params), status_success);
    EXPECT_FALSE(is_null(params.device));
    EXPECT_EQ(params.capabilities.max_texture_2d, 16384u);
    EXPECT_EQ(provider.last_request.registry_id, 7u);
    EXPECT_EQ(provider.last_request.heap_block_size, 1u << 20);
    EXPECT_EQ(provider.last_request.dedicated_threshold, 1u << 19);
}

TEST_F(HostTest, CreatesDeviceWithDefaultOptions) {
    initialize();
    create_device();
    const backend::DeviceRequest defaults{};
    EXPECT_EQ(provider.last_request.heap_block_size, defaults.heap_block_size);
    EXPECT_EQ(provider.last_request.dedicated_threshold, defaults.dedicated_threshold);
}

TEST_F(HostTest, ReportsDeviceCreationFailure) {
    initialize();
    provider.create_status = backend::Status::unsupported;
    CreateDeviceParams params{};
    EXPECT_EQ(call(FunctionCode::create_device, params), status_not_supported);
    CreateDeviceParams unaligned{0, {1000, 0}, {}, {}};
    EXPECT_EQ(call(FunctionCode::create_device, unaligned), status_invalid_parameter);
}

TEST_F(HostTest, CreatesAndValidatesBuffers) {
    initialize();
    device = create_device();
    EXPECT_FALSE(is_null(create_buffer(device, 256)));
    EXPECT_EQ(gpu.live_buffers, 1);
    CreateBufferParams empty{device, {0, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, empty), status_invalid_parameter);
    CreateBufferParams reserved{device, {16, backend::StorageMode::shared, {1}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, reserved), status_invalid_parameter);
    CreateBufferParams huge{device, {std::uint64_t{1} << 31, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, huge), status_invalid_parameter);
    CreateBufferParams memoryless{device, {16, backend::StorageMode::memoryless, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, memoryless), status_not_supported);
}

TEST_F(HostTest, ImportsAlignedMemoryOnly) {
    initialize();
    device = create_device();
    std::byte* memory = shared_memory(2 * shared_memory_alignment).data();
    ImportMemoryParams params{
        device, bridge::to_address(memory), 2 * shared_memory_alignment, {}, 0, 0};
    EXPECT_EQ(call(FunctionCode::import_memory, params), status_success);
    EXPECT_EQ(params.is_zero_copy, 1u);
    EXPECT_EQ(gpu.live_buffers, 1);
    ImportMemoryParams offset{
        device, bridge::to_address(memory + 8), shared_memory_alignment, {}, 0, 0};
    EXPECT_EQ(call(FunctionCode::import_memory, offset), status_invalid_parameter);
    ImportMemoryParams partial{device, bridge::to_address(memory), 100, {}, 0, 0};
    EXPECT_EQ(call(FunctionCode::import_memory, partial), status_invalid_parameter);
    ImportMemoryParams empty{device, bridge::to_address(memory), 0, {}, 0, 0};
    EXPECT_EQ(call(FunctionCode::import_memory, empty), status_invalid_parameter);
    ReleaseMemoryParams release{params.buffer};
    EXPECT_EQ(call(FunctionCode::release_memory, release), status_success);
    EXPECT_EQ(gpu.live_buffers, 0);
}

TEST_F(HostTest, ReportsTextureAllocationSize) {
    initialize();
    device = create_device();
    CreateTextureParams params{device, render_target_desc(), 0, {}, 0};
    EXPECT_EQ(call(FunctionCode::create_texture, params), status_success);
    EXPECT_FALSE(is_null(params.texture));
    EXPECT_EQ(params.allocation_size, 4096u);
    CreateTextureParams reserved{device, render_target_desc(), 1, {}, 0};
    EXPECT_EQ(call(FunctionCode::create_texture, reserved), status_invalid_parameter);
}

TEST_F(HostTest, CreatesTimelineWithMirror) {
    initialize();
    device = create_device();
    std::uint64_t mirror = 99;
    const Handle timeline = create_timeline(device, 5, &mirror);
    EXPECT_EQ(mirror, 5u);
    EXPECT_EQ(wait(timeline, 5), status_success);
    EXPECT_EQ(wait(timeline, 6), status_timeout);
    EXPECT_EQ(wait(timeline, 6, 1'000'000), status_timeout);
    EXPECT_FALSE(is_null(create_timeline(device, 0, nullptr)));
    alignas(8) std::byte storage[16] = {};
    CreateTimelineParams misaligned{device, 0, bridge::to_address(storage + 4), {}};
    EXPECT_EQ(call(FunctionCode::create_timeline, misaligned), status_invalid_parameter);
}

TEST_F(HostTest, RejectsAddressesAboveWow64Limit) {
    initialize(true);
    device = create_device();
    std::uint64_t mirror = 0;
    CreateTimelineParams timeline{device, 0, bridge::to_address(&mirror), {}};
    EXPECT_EQ(call(FunctionCode::create_timeline, timeline), status_invalid_parameter);
    EXPECT_FALSE(is_null(create_timeline(device, 0, nullptr)));
    OpenStreamParams opened{device, bridge::to_address(&control), {}, {}};
    EXPECT_EQ(call(FunctionCode::open_stream, opened), status_invalid_parameter);
    constexpr std::string_view name = "Present";
    LogCallParams log{
        bridge::to_address(name.data()), static_cast<std::uint32_t>(name.size()), 0, {}};
    EXPECT_EQ(call(FunctionCode::log_call, log), status_invalid_parameter);
}

TEST_F(HostTest, OpensStreamWithProgressTimeline) {
    initialize();
    device = create_device();
    control.retired_sequence = 77;
    stream = open_stream(device);
    EXPECT_FALSE(is_null(stream.stream));
    EXPECT_FALSE(is_null(stream.progress));
    EXPECT_EQ(control.retired_sequence, 0u);
    EXPECT_EQ(gpu.live_queues, 1);
    EXPECT_EQ(wait(stream.progress, 0), status_success);
    EXPECT_EQ(wait(stream.progress, 1), status_timeout);
    OpenStreamParams misaligned{device, bridge::to_address(&control) + 8, {}, {}};
    EXPECT_EQ(call(FunctionCode::open_stream, misaligned), status_invalid_parameter);
    CloseStreamParams close{stream.stream};
    EXPECT_EQ(call(FunctionCode::close_stream, close), status_success);
    EXPECT_EQ(gpu.live_queues, 0);
}

} // namespace
} // namespace directmetal::host::testing
