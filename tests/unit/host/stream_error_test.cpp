#include "host_fixture.hpp"

#include <directmetal/bridge/stream_decoder.hpp>

#include <future>

namespace directmetal::host::testing {
namespace {

using namespace bridge;

ClearTextureCommand clear_of(Handle texture) {
    return {texture, {0, 1, 0, 1}, {{1, 2, 3, 4}, backend::ClearKind::float_value, {}}, 0};
}

std::uint32_t raw(Opcode opcode) {
    return static_cast<std::uint32_t>(opcode);
}

class StreamErrorTest : public StreamTest {
protected:
    // Expects the stream to have failed at sequence with status and opcode, and every later chunk
    // to be ignored.
    void expect_sticky(Status status, std::uint32_t opcode, std::uint64_t sequence) {
        ASSERT_TRUE(wait_failed());
        EXPECT_EQ(control.status, status);
        EXPECT_EQ(control.failed_opcode, opcode);
        EXPECT_EQ(control.failed_sequence, sequence);
        const std::uint64_t retired = control.retired_sequence;
        submit(chunk(sequence + 1).add(clear_of(texture)));
        CloseStreamParams close{stream.stream};
        EXPECT_EQ(call(FunctionCode::close_stream, close), status_success);
        EXPECT_EQ(control.retired_sequence, retired);
        EXPECT_EQ(gpu.recorded().size(), recorded_before_failure);
    }

    void SetUp() override {
        StreamTest::SetUp();
        texture = create_texture(device, render_target_desc());
    }

    Handle texture{};
    std::size_t recorded_before_failure = 0;
};

TEST_F(StreamErrorTest, OutOfOrderSequenceIsSticky) {
    submit(chunk(2).add(clear_of(texture)));
    expect_sticky(status_invalid_parameter, framing_failure_opcode, 2);
}

TEST_F(StreamErrorTest, MismatchedSubmissionIsSticky) {
    Chunk& bad = chunk(1).add(clear_of(texture)).seal();
    SubmitStreamParams params = bad.submission(stream.stream);
    params.chunk_size -= 8;
    EXPECT_EQ(call(FunctionCode::submit_stream, params), status_success);
    expect_sticky(status_invalid_parameter, framing_failure_opcode, 1);
}

TEST_F(StreamErrorTest, MalformedChunkIsSticky) {
    Chunk& bad = chunk(1).add(clear_of(texture)).seal();
    bad.data()[0] = std::byte{0};
    SubmitStreamParams params = bad.submission(stream.stream);
    call(FunctionCode::submit_stream, params);
    expect_sticky(status_invalid_parameter, framing_failure_opcode, 1);
}

TEST_F(StreamErrorTest, ValidationFailureIsSticky) {
    backend::TextureDesc sampled = render_target_desc();
    sampled.usage = backend::TextureUsage::shader_read;
    const Handle unrenderable = create_texture(device, sampled);
    submit(chunk(1).add(clear_of(texture)));
    ASSERT_TRUE(wait_retired(1));
    recorded_before_failure = 1;
    submit(chunk(2).add(clear_of(unrenderable)).add(clear_of(texture)));
    expect_sticky(status_invalid_parameter, raw(Opcode::clear_texture), 2);
}

TEST_F(StreamErrorTest, StaleHandleIsSticky) {
    submit(chunk(1).add(DestroyObjectCommand{texture}).add(clear_of(texture)));
    expect_sticky(status_invalid_handle, raw(Opcode::clear_texture), 1);
}

TEST_F(StreamErrorTest, ForeignDeviceHandleIsSticky) {
    const Handle other = create_device();
    const Handle foreign = create_texture(other, render_target_desc());
    submit(chunk(1).add(clear_of(foreign)));
    expect_sticky(status_invalid_handle, raw(Opcode::clear_texture), 1);
}

TEST_F(StreamErrorTest, DestroyingImportedBufferIsSticky) {
    const std::span memory = shared_memory(shared_memory_alignment);
    ImportMemoryParams imported{device, bridge::to_address(memory.data()), memory.size(), {}, 0, 0};
    ASSERT_EQ(call(FunctionCode::import_memory, imported), status_success);
    submit(chunk(1).add(DestroyObjectCommand{imported.buffer}));
    expect_sticky(status_invalid_parameter, raw(Opcode::destroy_object), 1);
}

TEST_F(StreamErrorTest, DestroyingTimelineIsSticky) {
    submit(chunk(1).add(DestroyObjectCommand{stream.progress}));
    expect_sticky(status_invalid_parameter, raw(Opcode::destroy_object), 1);
}

TEST_F(StreamErrorTest, SignalingProgressIsSticky) {
    submit(chunk(1).add(SignalTimelineCommand{stream.progress, 1}));
    expect_sticky(status_invalid_parameter, raw(Opcode::signal_timeline), 1);
}

TEST_F(StreamErrorTest, LoweringTimelineIsSticky) {
    const Handle timeline = create_timeline(device, 5, nullptr);
    submit(chunk(1).add(SignalTimelineCommand{timeline, 4}));
    expect_sticky(status_invalid_parameter, raw(Opcode::signal_timeline), 1);
}

TEST_F(StreamErrorTest, OutOfBoundsCopyIsSticky) {
    const Handle buffer = create_buffer(device, 255);
    const BufferTextureRegion region{buffer, 0, 32, 256, texture, 0, 0, {0, 0, 0}, {8, 8, 1}};
    submit(chunk(1).add(CopyTextureToBufferCommand{region}));
    expect_sticky(status_invalid_parameter, raw(Opcode::copy_texture_to_buffer), 1);
}

TEST_F(StreamErrorTest, OverlappingBufferCopyIsSticky) {
    const Handle buffer = create_buffer(device, 256);
    submit(chunk(1).add(CopyBufferCommand{buffer, 0, buffer, 64, 128}));
    expect_sticky(status_invalid_parameter, raw(Opcode::copy_buffer), 1);
}

TEST_F(StreamErrorTest, UnalignedBufferCopyIsSticky) {
    const Handle source = create_buffer(device, 256);
    const Handle destination = create_buffer(device, 256);
    submit(chunk(1).add(CopyBufferCommand{source, 0, destination, 0, 64}));
    ASSERT_TRUE(wait_retired(1));
    recorded_before_failure = 1;
    submit(chunk(2).add(CopyBufferCommand{source, 2, destination, 0, 64}));
    expect_sticky(status_invalid_parameter, raw(Opcode::copy_buffer), 2);
}

TEST_F(StreamErrorTest, UnalignedBufferCopySizeIsSticky) {
    const Handle source = create_buffer(device, 256);
    const Handle destination = create_buffer(device, 256);
    submit(chunk(1).add(CopyBufferCommand{source, 0, destination, 4, 6}));
    expect_sticky(status_invalid_parameter, raw(Opcode::copy_buffer), 1);
}

TEST_F(StreamErrorTest, InvalidSubresourceIsSticky) {
    ClearTextureCommand clear = clear_of(texture);
    clear.range = {1, 2, 0, 1};
    submit(chunk(1).add(clear));
    expect_sticky(status_invalid_parameter, raw(Opcode::clear_texture), 1);
}

TEST_F(StreamErrorTest, WaitsFailAfterStickyError) {
    const Handle timeline = create_timeline(device, 3, nullptr);
    EXPECT_EQ(wait(timeline, 3), status_success);
    // Finite, so a wait that misses the error fails the test instead of hanging it.
    constexpr std::uint64_t five_seconds = 5'000'000'000;
    auto blocked =
        std::async(std::launch::async, [&] { return wait(stream.progress, 1, five_seconds); });
    EXPECT_EQ(blocked.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    submit(chunk(2));
    ASSERT_TRUE(wait_failed());
    EXPECT_EQ(blocked.get(), status_device_removed);
    EXPECT_EQ(wait(stream.progress, 0), status_device_removed);
    EXPECT_EQ(wait(timeline, 3), status_device_removed);
}

} // namespace
} // namespace directmetal::host::testing
