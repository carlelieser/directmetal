#include "host_fixture.hpp"

#include <set>

namespace directmetal::host::testing {
namespace {

using namespace bridge;

TEST_F(HostTest, NeverReusesHandles) {
    initialize();
    device = create_device();
    std::set<std::uint64_t> seen{device.value};
    for (int round = 0; round < 100; ++round) {
        const Handle timeline = create_timeline(device, 0, nullptr);
        EXPECT_TRUE(seen.insert(timeline.value).second) << "round " << round;
        DestroyTimelineParams destroy{timeline};
        ASSERT_EQ(call(FunctionCode::destroy_timeline, destroy), status_success);
        EXPECT_EQ(call(FunctionCode::destroy_timeline, destroy), status_invalid_handle);
        EXPECT_EQ(wait(timeline, 0), status_invalid_handle);
    }
}

TEST_F(HostTest, RejectsNullAndForgedHandles) {
    initialize();
    device = create_device();
    // Occupies the slot after the device's, so a forged index names a live object.
    create_buffer(device, 64);
    CreateBufferParams null_device{{}, {64, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, null_device), status_invalid_handle);
    CreateBufferParams forged{{device.value + 1}, {64, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, forged), status_invalid_handle);
    CreateBufferParams other_generation{
        {device.value ^ (std::uint64_t{1} << 33)}, {64, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, other_generation), status_invalid_handle);
    WaitTimelineParams unknown{{0xFFFFFFFFFFFFFFFF}, 0, 0};
    EXPECT_EQ(call(FunctionCode::wait_timeline, unknown), status_invalid_handle);
}

TEST_F(HostTest, RejectsHandlesOfAnotherKind) {
    initialize();
    device = create_device();
    const Handle buffer = create_buffer(device, 64);
    stream = open_stream(device);
    CreateBufferParams by_buffer{buffer, {64, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, by_buffer), status_invalid_handle);
    DestroyTimelineParams progress{stream.progress};
    EXPECT_EQ(call(FunctionCode::destroy_timeline, progress), status_invalid_handle);
    ReleaseMemoryParams created{buffer};
    EXPECT_EQ(call(FunctionCode::release_memory, created), status_invalid_handle);
    WaitTimelineParams by_stream{stream.stream, 0, 0};
    EXPECT_EQ(call(FunctionCode::wait_timeline, by_stream), status_invalid_handle);
    CloseStreamParams by_device{device};
    EXPECT_EQ(call(FunctionCode::close_stream, by_device), status_invalid_handle);
    DestroyDeviceParams by_stream_device{stream.stream};
    EXPECT_EQ(call(FunctionCode::destroy_device, by_stream_device), status_invalid_handle);
    SubmitStreamParams by_progress{stream.progress, 0, 0, 0, 1};
    EXPECT_EQ(call(FunctionCode::submit_stream, by_progress), status_invalid_handle);
}

TEST_F(HostTest, StalesEveryHandleOfDestroyedDevice) {
    initialize();
    device = create_device();
    const Handle buffer = create_buffer(device, 64);
    const Handle timeline = create_timeline(device, 0, nullptr);
    stream = open_stream(device);
    DestroyDeviceParams destroy{device};
    ASSERT_EQ(call(FunctionCode::destroy_device, destroy), status_success);
    EXPECT_EQ(call(FunctionCode::destroy_device, destroy), status_invalid_handle);
    EXPECT_EQ(wait(timeline, 0), status_invalid_handle);
    EXPECT_EQ(wait(stream.progress, 0), status_invalid_handle);
    CreateBufferParams on_destroyed{device, {64, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, on_destroyed), status_invalid_handle);
    CloseStreamParams close{stream.stream};
    EXPECT_EQ(call(FunctionCode::close_stream, close), status_invalid_handle);
    EXPECT_EQ(gpu.live_buffers, 0);
    EXPECT_EQ(gpu.live_queues, 0);
    const Handle replacement = create_device();
    for (const Handle stale : {device, buffer, timeline, stream.stream, stream.progress}) {
        EXPECT_NE(replacement.value, stale.value);
    }
}

} // namespace
} // namespace directmetal::host::testing
