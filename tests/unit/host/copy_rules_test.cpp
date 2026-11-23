#include "host_fixture.hpp"

#include <directmetal/bridge/stream_decoder.hpp>

namespace directmetal::host::testing {
namespace {

using namespace bridge;
using backend::PixelFormat;
using backend::TextureDesc;
using backend::TextureType;
using backend::TextureUsage;

std::uint32_t raw(Opcode opcode) {
    return static_cast<std::uint32_t>(opcode);
}

class CopyRulesTest : public StreamTest {
protected:
    // Submits command alone in the next chunk and reports whether the stream accepted it.
    template <typename Command> bool accepts(const Command& command) {
        const std::uint64_t sequence = next_sequence++;
        submit(chunk(sequence).add(command));
        EXPECT_TRUE(wait_retired(sequence));
        return std::atomic_ref(control.status).load() == status_success;
    }

    static TextureDesc bc1_desc() {
        return {TextureType::tex2d,
                backend::StorageMode::gpu_private,
                0,
                PixelFormat::bc1_rgba,
                {16, 16, 1},
                3,
                1,
                1,
                TextureUsage::shader_read};
    }

    std::uint64_t next_sequence = 1;
};

TEST_F(CopyRulesTest, AcceptsWholeCompressedBlocks) {
    const Handle texture = create_texture(device, bc1_desc());
    const Handle buffer = create_buffer(device, 4096);
    // 16x16 BC1: 4x4 blocks of 8 bytes, so a row of blocks is 32 bytes.
    EXPECT_TRUE(accepts(
        CopyBufferToTextureCommand{{buffer, 0, 32, 128, texture, 0, 0, {4, 8, 0}, {8, 8, 1}}}));
    // Mip 2 is 4x4, a single block.
    EXPECT_TRUE(accepts(
        CopyBufferToTextureCommand{{buffer, 0, 8, 8, texture, 2, 0, {0, 0, 0}, {4, 4, 1}}}));
}

TEST_F(CopyRulesTest, RejectsCompressedOriginInsideBlock) {
    const Handle texture = create_texture(device, bc1_desc());
    const Handle buffer = create_buffer(device, 4096);
    EXPECT_FALSE(accepts(
        CopyBufferToTextureCommand{{buffer, 0, 32, 128, texture, 0, 0, {2, 0, 0}, {8, 8, 1}}}));
    EXPECT_EQ(control.failed_opcode, raw(Opcode::copy_buffer_to_texture));
}

TEST_F(CopyRulesTest, RejectsPartialCompressedBlockAwayFromEdge) {
    const Handle texture = create_texture(device, bc1_desc());
    const Handle buffer = create_buffer(device, 4096);
    EXPECT_FALSE(accepts(
        CopyBufferToTextureCommand{{buffer, 0, 32, 128, texture, 0, 0, {0, 0, 0}, {6, 4, 1}}}));
}

TEST_F(CopyRulesTest, RejectsOverlappingTextureCopy) {
    const Handle texture = create_texture(device, render_target_desc());
    EXPECT_TRUE(accepts(
        CopyTextureCommand{texture, 0, 0, {0, 0, 0}, 0, texture, 0, 0, {4, 4, 0}, {4, 4, 1}, {}}));
    EXPECT_TRUE(accepts(
        CopyTextureCommand{texture, 0, 0, {0, 0, 0}, 0, texture, 1, 0, {0, 0, 0}, {4, 4, 1}, {}}));
    EXPECT_FALSE(accepts(
        CopyTextureCommand{texture, 0, 0, {0, 0, 0}, 0, texture, 0, 0, {2, 2, 0}, {4, 4, 1}, {}}));
    EXPECT_EQ(control.failed_opcode, raw(Opcode::copy_texture));
}

TEST_F(CopyRulesTest, RejectsRowPitchShorterThanARow) {
    const Handle texture = create_texture(device, render_target_desc());
    const Handle buffer = create_buffer(device, 4096);
    EXPECT_TRUE(accepts(
        CopyTextureToBufferCommand{{buffer, 0, 32, 0, texture, 0, 0, {0, 0, 0}, {8, 8, 1}}}));
    EXPECT_FALSE(accepts(
        CopyTextureToBufferCommand{{buffer, 0, 28, 0, texture, 0, 0, {0, 0, 0}, {8, 8, 1}}}));
}

TEST_F(CopyRulesTest, RejectsRowPitchOffTexelBoundary) {
    const Handle texture = create_texture(device, render_target_desc());
    const Handle buffer = create_buffer(device, 4096);
    EXPECT_FALSE(accepts(
        CopyTextureToBufferCommand{{buffer, 0, 34, 0, texture, 0, 0, {0, 0, 0}, {8, 8, 1}}}));
}

TEST_F(CopyRulesTest, RejectsSlicePitchShorterThanAnImage) {
    TextureDesc volume = render_target_desc();
    volume.type = TextureType::tex3d;
    volume.extent = {8, 8, 4};
    volume.mip_levels = 1;
    volume.usage = TextureUsage::shader_read;
    const Handle texture = create_texture(device, volume);
    const Handle buffer = create_buffer(device, 4096);
    EXPECT_TRUE(accepts(
        CopyTextureToBufferCommand{{buffer, 0, 32, 256, texture, 0, 0, {0, 0, 0}, {8, 8, 4}}}));
    EXPECT_FALSE(accepts(
        CopyTextureToBufferCommand{{buffer, 0, 32, 192, texture, 0, 0, {0, 0, 0}, {8, 8, 4}}}));
}

TEST_F(CopyRulesTest, RejectsCopyPastBufferEnd) {
    const Handle texture = create_texture(device, render_target_desc());
    const Handle buffer = create_buffer(device, 256);
    EXPECT_TRUE(accepts(
        CopyTextureToBufferCommand{{buffer, 0, 32, 0, texture, 0, 0, {0, 0, 0}, {8, 8, 1}}}));
    EXPECT_FALSE(accepts(
        CopyTextureToBufferCommand{{buffer, 4, 32, 0, texture, 0, 0, {0, 0, 0}, {8, 8, 1}}}));
}

TEST_F(HostTest, RejectsImportAboveWow64Limit) {
    initialize(true);
    device = create_device();
    const std::span memory = shared_memory(shared_memory_alignment);
    ASSERT_GE(bridge::to_address(memory.data()), std::uint64_t{1} << 32);
    ImportMemoryParams params{device, bridge::to_address(memory.data()), memory.size(), {}, 0, 0};
    EXPECT_EQ(call(FunctionCode::import_memory, params), status_invalid_parameter);
    EXPECT_EQ(gpu.live_buffers, 0);
}

} // namespace
} // namespace directmetal::host::testing
