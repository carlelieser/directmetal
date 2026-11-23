#include "host_fixture.hpp"

#include <functional>
#include <string>

namespace directmetal::host::testing {
namespace {

using namespace bridge;
using backend::PixelFormat;
using backend::StorageMode;
using backend::TextureDesc;
using backend::TextureType;
using backend::TextureUsage;

struct DescCase {
    std::string name;
    std::function<void(TextureDesc&)> change;
};

class TextureDescTest : public HostTest, public ::testing::WithParamInterface<DescCase> {
protected:
    Status create(const TextureDesc& desc) {
        CreateTextureParams params{device, desc, 0, {}, 0};
        return call(FunctionCode::create_texture, params);
    }
};

TEST_P(TextureDescTest, RejectsInvalidDescription) {
    initialize();
    device = create_device();
    TextureDesc desc = render_target_desc();
    ASSERT_EQ(create(desc), status_success);
    GetParam().change(desc);
    EXPECT_EQ(create(desc), status_invalid_parameter);
    EXPECT_EQ(gpu.live_textures, 1);
}

INSTANTIATE_TEST_SUITE_P(
    Descriptions, TextureDescTest,
    ::testing::Values(
        DescCase{"UnknownType", [](TextureDesc& desc) { desc.type = static_cast<TextureType>(9); }},
        DescCase{"UnknownStorage",
                 [](TextureDesc& desc) { desc.storage = static_cast<StorageMode>(3); }},
        DescCase{"Reserved", [](TextureDesc& desc) { desc.reserved = 1; }},
        DescCase{"UnknownFormat",
                 [](TextureDesc& desc) { desc.format = static_cast<PixelFormat>(9999); }},
        DescCase{"InvalidFormat", [](TextureDesc& desc) { desc.format = PixelFormat::invalid; }},
        DescCase{"UnknownUsage",
                 [](TextureDesc& desc) { desc.usage = static_cast<TextureUsage>(0x8); }},
        DescCase{"ZeroWidth", [](TextureDesc& desc) { desc.extent.width = 0; }},
        DescCase{"DepthOn2D", [](TextureDesc& desc) { desc.extent.depth = 2; }},
        DescCase{"HeightOn1D",
                 [](TextureDesc& desc) {
                     desc.type = TextureType::tex1d;
                     desc.mip_levels = 1;
                 }},
        DescCase{"NonSquareCube",
                 [](TextureDesc& desc) {
                     desc.type = TextureType::cube;
                     desc.extent.height = 4;
                 }},
        DescCase{"TooWide", [](TextureDesc& desc) { desc.extent.width = 16385; }},
        DescCase{"ZeroMips", [](TextureDesc& desc) { desc.mip_levels = 0; }},
        DescCase{"TooManyMips", [](TextureDesc& desc) { desc.mip_levels = 5; }},
        DescCase{"ArrayOn2D", [](TextureDesc& desc) { desc.array_length = 2; }},
        DescCase{"ZeroArray",
                 [](TextureDesc& desc) {
                     desc.type = TextureType::tex2d_array;
                     desc.array_length = 0;
                 }},
        DescCase{"SamplesOn2D", [](TextureDesc& desc) { desc.sample_count = 4; }},
        DescCase{"SingleSampleMultisample",
                 [](TextureDesc& desc) {
                     desc.type = TextureType::tex2d_ms;
                     desc.mip_levels = 1;
                 }},
        DescCase{"MipmappedMultisample",
                 [](TextureDesc& desc) {
                     desc.type = TextureType::tex2d_ms;
                     desc.sample_count = 4;
                 }},
        DescCase{"CompressedRenderTarget",
                 [](TextureDesc& desc) { desc.format = PixelFormat::bc1_rgba; }},
        DescCase{"Depth3D",
                 [](TextureDesc& desc) {
                     desc.type = TextureType::tex3d;
                     desc.format = PixelFormat::depth32_float;
                 }}),
    [](const auto& info) { return info.param.name; });

TEST_F(HostTest, AcceptsTextureTypes) {
    initialize();
    device = create_device();
    TextureDesc cube = render_target_desc();
    cube.type = TextureType::cube_array;
    cube.array_length = 2;
    EXPECT_FALSE(is_null(create_texture(device, cube)));
    TextureDesc volume = render_target_desc();
    volume.type = TextureType::tex3d;
    volume.extent = {4, 4, 16};
    volume.mip_levels = 5;
    EXPECT_FALSE(is_null(create_texture(device, volume)));
    TextureDesc multisample = render_target_desc();
    multisample.type = TextureType::tex2d_ms;
    multisample.sample_count = 4;
    multisample.mip_levels = 1;
    EXPECT_FALSE(is_null(create_texture(device, multisample)));
    TextureDesc compressed = render_target_desc();
    compressed.format = PixelFormat::bc7_rgbaunorm;
    compressed.usage = TextureUsage::shader_read;
    EXPECT_FALSE(is_null(create_texture(device, compressed)));
}

} // namespace
} // namespace directmetal::host::testing
