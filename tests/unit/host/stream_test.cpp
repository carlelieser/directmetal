#include "host_fixture.hpp"

#include <gmock/gmock.h>

namespace directmetal::host::testing {
namespace {

using namespace bridge;
using ::testing::ElementsAre;

ClearTextureCommand clear_of(Handle texture) {
    return {texture, {0, 1, 0, 1}, {{1, 2, 3, 4}, backend::ClearKind::uint_value, {}}, 0};
}

Status status_of(const StreamControl& control) {
    return std::atomic_ref(const_cast<Status&>(control.status)).load();
}

TEST_F(StreamTest, RetiresChunksInSequence) {
    const Handle texture = create_texture(device, render_target_desc());
    EXPECT_EQ(submit(chunk(1).add(clear_of(texture))), status_success);
    EXPECT_EQ(submit(chunk(2).add(SubmitCommand{})), status_success);
    EXPECT_EQ(submit(chunk(3).add(clear_of(texture))), status_success);
    ASSERT_TRUE(wait_retired(3));
    EXPECT_EQ(control.retired_sequence, 3u);
    EXPECT_EQ(status_of(control), status_success);
    EXPECT_THAT(gpu.recorded(), ElementsAre("clear_texture", "clear_texture"));
}

TEST_F(StreamTest, SignalsProgressOnSubmit) {
    gpu.is_auto_complete = false;
    submit(chunk(1).add(SubmitCommand{}).add(SubmitCommand{}));
    ASSERT_TRUE(wait_retired(1));
    EXPECT_EQ(gpu.submitted(0), 2u);
    EXPECT_EQ(wait(stream.progress, 2), status_timeout);
    EXPECT_EQ(control.completed_progress, 0u);
    gpu.complete_all();
    EXPECT_EQ(wait(stream.progress, 2), status_success);
    EXPECT_EQ(std::atomic_ref(control.completed_progress).load(), 2u);
}

TEST_F(StreamTest, SignalsTimelinesAndMirrors) {
    std::uint64_t mirror = 0;
    const Handle timeline = create_timeline(device, 1, &mirror);
    submit(chunk(1).add(SignalTimelineCommand{timeline, 4}).add(SubmitCommand{}));
    ASSERT_TRUE(wait_retired(1));
    EXPECT_EQ(wait(timeline, 4, timeout_infinite), status_success);
    EXPECT_EQ(std::atomic_ref(mirror).load(), 4u);
    EXPECT_THAT(gpu.recorded(), ElementsAre("signal 4"));
}

TEST_F(StreamTest, RecordsEveryCopyKind) {
    const Handle texture = create_texture(device, render_target_desc());
    const Handle other = create_texture(device, render_target_desc());
    const Handle buffer = create_buffer(device, 1024);
    const Handle staging = create_buffer(device, 1024);
    const BufferTextureRegion region{buffer, 0, 32, 256, texture, 0, 0, {0, 0, 0}, {8, 8, 1}};
    const CopyTextureCommand copy{texture, 0, 0,         {0, 0, 0}, 0, other,
                                  1,       0, {0, 0, 0}, {4, 4, 1}, {}};
    submit(chunk(1)
               .add(CopyBufferCommand{buffer, 0, staging, 512, 512})
               .add(CopyBufferToTextureCommand{region})
               .add(CopyTextureToBufferCommand{region})
               .add(copy)
               .add_label(buffer, "staging")
               .add(SubmitCommand{}));
    ASSERT_TRUE(wait_retired(1));
    EXPECT_EQ(status_of(control), status_success);
    EXPECT_THAT(gpu.recorded(),
                ElementsAre("copy_buffer", "copy_buffer_to_texture", "copy_texture_to_buffer",
                            "copy_texture", "label staging"));
}

TEST_F(StreamTest, ClearsDepthStencil) {
    backend::TextureDesc desc = render_target_desc();
    desc.format = backend::PixelFormat::depth32_float_stencil8;
    desc.mip_levels = 1;
    const Handle texture = create_texture(device, desc);
    const auto both = static_cast<backend::ClearAspect>(0x3);
    submit(chunk(1).add(ClearDepthStencilCommand{texture, {0, 1, 0, 1}, {0.5f, 7, both, 0}}));
    ASSERT_TRUE(wait_retired(1));
    EXPECT_EQ(status_of(control), status_success);
    EXPECT_THAT(gpu.recorded(), ElementsAre("clear_depth_stencil"));
}

TEST_F(StreamTest, DestroysObjectsAfterTheirLastSubmit) {
    gpu.is_auto_complete = false;
    const Handle texture = create_texture(device, render_target_desc());
    submit(chunk(1).add(clear_of(texture)).add(DestroyObjectCommand{texture}));
    ASSERT_TRUE(wait_retired(1));
    EXPECT_EQ(gpu.live_textures, 1);
    submit(chunk(2).add(SubmitCommand{}));
    ASSERT_TRUE(wait_retired(2));
    EXPECT_EQ(gpu.live_textures, 0);
    EXPECT_EQ(status_of(control), status_success);
}

TEST_F(StreamTest, RejectsQueuedCommandsAfterStreamCloses) {
    CloseStreamParams close{stream.stream};
    ASSERT_EQ(call(FunctionCode::close_stream, close), status_success);
    SubmitStreamParams params = chunk(1).add(SubmitCommand{}).seal().submission(stream.stream);
    EXPECT_EQ(call(FunctionCode::submit_stream, params), status_invalid_handle);
    EXPECT_EQ(wait(stream.progress, 0), status_invalid_handle);
}

} // namespace
} // namespace directmetal::host::testing
