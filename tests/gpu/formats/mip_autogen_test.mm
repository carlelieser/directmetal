#include "format_fixture.hpp"

#include "resource_access.hpp"

// Formats claiming mip_autogen generate the mips of a 2D texture created with their claimed
// usages through MTLBlitCommandEncoder generateMipmapsForTexture.
namespace directmetal::formats::gpu_testing {
namespace {

constexpr std::uint32_t mip_levels = 4;

class MipAutogenTest : public FormatGpuTest,
                       public ::testing::WithParamInterface<const FormatInfo*> {
protected:
    const FormatInfo& info() const { return *GetParam(); }
};

TEST_P(MipAutogenTest, GeneratesMipsOfClaimedFormat) {
    const Support claimed = support(info().format, features());
    if (!has_any(claimed, Support::mip_autogen)) {
        GTEST_SKIP() << "no mip generation claimed on this device";
    }
    const auto texture = make_texture({backend::TextureType::tex2d,
                                       backend::StorageMode::gpu_private,
                                       0,
                                       resource_pixel_format(info()),
                                       {8, 8, 1},
                                       mip_levels,
                                       1,
                                       1,
                                       claimed_usage(info(), claimed)});
    ASSERT_NE(texture, nullptr);
    id<MTLCommandBuffer> commands = [[metal_device() newCommandQueue] commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    [blit generateMipmapsForTexture:backend::resources::metal_texture(*texture)];
    [blit endEncoding];
    [commands commit];
    [commands waitUntilCompleted];
    EXPECT_EQ(commands.status, MTLCommandBufferStatusCompleted);
}

INSTANTIATE_TEST_SUITE_P(DxgiFormats, MipAutogenTest, ::testing::ValuesIn(every_format()),
                         format_test_name);

} // namespace
} // namespace directmetal::formats::gpu_testing
