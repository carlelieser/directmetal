#include "host_fixture.hpp"

#include <future>

namespace directmetal::host::testing {
namespace {

using namespace bridge;

constexpr auto still_blocked = std::chrono::milliseconds(50);

class TeardownTest : public StreamTest {
protected:
    void SetUp() override {
        StreamTest::SetUp();
        gpu.is_auto_complete = false;
        texture = create_texture(device, render_target_desc());
        const std::span memory = shared_memory(shared_memory_alignment);
        ImportMemoryParams params{device, bridge::to_address(memory.data()), memory.size(), {}, 0,
                                  0};
        ASSERT_EQ(call(FunctionCode::import_memory, params), status_success);
        imported = params.buffer;
    }

    // A readback into the imported buffer.
    Chunk& readback(std::uint64_t sequence) {
        const BufferTextureRegion region{imported, 0, 32, 256, texture, 0, 0, {0, 0, 0}, {8, 8, 1}};
        return chunk(sequence).add(CopyTextureToBufferCommand{region});
    }

    template <typename Params> std::future<Status> call_async(FunctionCode code, Params params) {
        return std::async(std::launch::async,
                          [this, code, params]() mutable { return call(code, params); });
    }

    Handle texture{};
    Handle imported{};
};

TEST_F(TeardownTest, ReleaseMemoryWaitsForSubmittedWork) {
    submit(readback(1).add(SubmitCommand{}));
    auto released = call_async(FunctionCode::release_memory, ReleaseMemoryParams{imported});
    EXPECT_EQ(released.wait_for(still_blocked), std::future_status::timeout);
    EXPECT_EQ(gpu.live_buffers, 1);
    gpu.complete_all();
    EXPECT_EQ(released.get(), status_success);
    EXPECT_EQ(gpu.live_buffers, 0);
}

TEST_F(TeardownTest, ReleaseMemoryRefusesUnsubmittedWork) {
    // Completing at once, so an implicit commit would return instead of hanging the test.
    gpu.is_auto_complete = true;
    submit(readback(1));
    ReleaseMemoryParams release{imported};
    EXPECT_EQ(call(FunctionCode::release_memory, release), status_invalid_device_state);
    EXPECT_EQ(control.retired_sequence, 1u);
    EXPECT_EQ(gpu.submitted(0), 0u);
    EXPECT_EQ(gpu.live_buffers, 1);
    gpu.is_auto_complete = false;
    submit(chunk(2).add(SubmitCommand{}));
    auto released = call_async(FunctionCode::release_memory, release);
    EXPECT_EQ(released.wait_for(still_blocked), std::future_status::timeout);
    gpu.complete_all();
    EXPECT_EQ(released.get(), status_success);
    EXPECT_EQ(gpu.submitted(0), 1u);
    EXPECT_EQ(gpu.live_buffers, 0);
}

TEST_F(TeardownTest, ReleaseMemorySettlesFailedStream) {
    gpu.is_auto_complete = true;
    submit(readback(1));
    ASSERT_TRUE(wait_retired(1));
    submit(chunk(3).add(SubmitCommand{}));
    ASSERT_TRUE(wait_failed());
    submit(chunk(2).add(SubmitCommand{}));
    ReleaseMemoryParams release{imported};
    EXPECT_EQ(call(FunctionCode::release_memory, release), status_success);
    EXPECT_EQ(gpu.live_buffers, 0);
}

TEST_F(TeardownTest, CloseStreamCommitsRecordedWork) {
    submit(readback(1));
    auto closed = call_async(FunctionCode::close_stream, CloseStreamParams{stream.stream});
    EXPECT_EQ(closed.wait_for(still_blocked), std::future_status::timeout);
    EXPECT_EQ(gpu.submitted(0), 1u);
    gpu.complete_all();
    EXPECT_EQ(closed.get(), status_success);
    EXPECT_EQ(std::atomic_ref(control.completed_progress).load(), 1u);
}

TEST_F(TeardownTest, DestroyDeviceCommitsRecordedWork) {
    submit(readback(1));
    auto destroyed = call_async(FunctionCode::destroy_device, DestroyDeviceParams{device});
    EXPECT_EQ(destroyed.wait_for(still_blocked), std::future_status::timeout);
    EXPECT_EQ(gpu.submitted(0), 1u);
    gpu.complete_all();
    EXPECT_EQ(destroyed.get(), status_success);
    EXPECT_EQ(gpu.live_queues, 0);
}

TEST_F(TeardownTest, CloseStreamDecodesQueuedChunks) {
    gpu.is_auto_complete = true;
    for (std::uint64_t sequence = 1; sequence <= 20; ++sequence) {
        submit(readback(sequence));
    }
    CloseStreamParams close{stream.stream};
    EXPECT_EQ(call(FunctionCode::close_stream, close), status_success);
    EXPECT_EQ(control.retired_sequence, 20u);
    EXPECT_EQ(gpu.recorded().size(), 20u);
    EXPECT_EQ(gpu.live_queues, 0);
}

TEST_F(TeardownTest, DestroyDeviceWaitsAndReleasesEverything) {
    create_buffer(device, 64);
    create_timeline(device, 0, nullptr);
    submit(readback(1).add(SubmitCommand{}));
    auto destroyed = call_async(FunctionCode::destroy_device, DestroyDeviceParams{device});
    EXPECT_EQ(destroyed.wait_for(still_blocked), std::future_status::timeout);
    gpu.complete_all();
    EXPECT_EQ(destroyed.get(), status_success);
    EXPECT_EQ(gpu.live_buffers, 0);
    EXPECT_EQ(gpu.live_textures, 0);
    EXPECT_EQ(gpu.live_queues, 0);
}

TEST_F(TeardownTest, DestroyTimelineDetachesMirror) {
    std::uint64_t mirror = 0;
    const Handle timeline = create_timeline(device, 1, &mirror);
    submit(chunk(1).add(SignalTimelineCommand{timeline, 9}).add(SubmitCommand{}));
    ASSERT_TRUE(wait_retired(1));
    DestroyTimelineParams destroy{timeline};
    ASSERT_EQ(call(FunctionCode::destroy_timeline, destroy), status_success);
    gpu.complete_all();
    EXPECT_EQ(std::atomic_ref(mirror).load(), 1u);
    EXPECT_EQ(std::atomic_ref(control.completed_progress).load(), 1u);
}

TEST_F(TeardownTest, CloseStreamDetachesControl) {
    gpu.is_auto_complete = true;
    submit(chunk(1).add(SubmitCommand{}));
    CloseStreamParams close{stream.stream};
    ASSERT_EQ(call(FunctionCode::close_stream, close), status_success);
    EXPECT_EQ(control.completed_progress, 1u);
    control = {};
    gpu.complete_all();
    EXPECT_EQ(control.completed_progress, 0u);
    EXPECT_EQ(control.retired_sequence, 0u);
}

TEST_F(TeardownTest, HostDestructionTearsDownOpenDevices) {
    gpu.is_auto_complete = true;
    submit(readback(1).add(SubmitCommand{}));
    host.reset();
    EXPECT_EQ(gpu.live_buffers, 0);
    EXPECT_EQ(gpu.live_textures, 0);
    EXPECT_EQ(gpu.live_queues, 0);
    EXPECT_EQ(control.retired_sequence, 1u);
}

} // namespace
} // namespace directmetal::host::testing
