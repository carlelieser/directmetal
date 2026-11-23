#include "device_fixture.hpp"
#include "resource_access.hpp"
#include "texture_descriptor.hpp"

#include <array>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

struct TextureCase {
    TextureDesc desc;
    MTLTextureType metal_type;
    NSUInteger metal_array_length;
};

constexpr TextureUsage sampled = TextureUsage::shader_read;
constexpr TextureUsage target = TextureUsage::shader_read | TextureUsage::render_target;

constexpr StorageMode gpu = StorageMode::gpu_private;
constexpr PixelFormat rgba8 = PixelFormat::rgba8_unorm;

const std::array texture_cases{
    TextureCase{
        {TextureType::tex1d, gpu, 0, rgba8, {64, 1, 1}, 1, 1, 1, sampled}, MTLTextureType1D, 1},
    TextureCase{{TextureType::tex1d_array, gpu, 0, rgba8, {64, 1, 1}, 1, 3, 1, sampled},
                MTLTextureType1DArray,
                3},
    TextureCase{
        {TextureType::tex2d, gpu, 0, rgba8, {64, 32, 1}, 7, 1, 1, target}, MTLTextureType2D, 1},
    TextureCase{
        {TextureType::tex2d_array, gpu, 0, PixelFormat::rgba16_float, {16, 16, 1}, 5, 4, 1, target},
        MTLTextureType2DArray,
        4},
    TextureCase{{TextureType::tex2d_ms, gpu, 0, rgba8, {16, 16, 1}, 1, 1, 4, target},
                MTLTextureType2DMultisample,
                1},
    TextureCase{{TextureType::tex2d_ms_array, gpu, 0, rgba8, {16, 16, 1}, 1, 3, 4, target},
                MTLTextureType2DMultisampleArray,
                3},
    TextureCase{{TextureType::tex3d, gpu, 0, PixelFormat::r32_float, {8, 8, 8}, 4, 1, 1, sampled},
                MTLTextureType3D,
                1},
    TextureCase{
        {TextureType::cube, gpu, 0, rgba8, {16, 16, 1}, 5, 1, 1, target}, MTLTextureTypeCube, 1},
    TextureCase{{TextureType::cube_array, gpu, 0, rgba8, {16, 16, 1}, 1, 2, 1, sampled},
                MTLTextureTypeCubeArray,
                2},
    TextureCase{{TextureType::tex2d,
                 StorageMode::shared,
                 0,
                 PixelFormat::bgra8_unorm_srgb,
                 {32, 32, 1},
                 1,
                 1,
                 1,
                 target},
                MTLTextureType2D,
                1},
    TextureCase{
        {TextureType::tex2d, gpu, 0, PixelFormat::depth32_float, {32, 32, 1}, 1, 1, 1, target},
        MTLTextureType2D,
        1},
    TextureCase{{TextureType::tex2d, gpu, 0, PixelFormat::bc1_rgba, {64, 64, 1}, 3, 1, 1, sampled},
                MTLTextureType2D,
                1},
};

TEST(Textures, PlacesEveryTypeInAHeapAsDescribed) {
    const auto device = make_device();
    for (const TextureCase& texture_case : texture_cases) {
        const TextureDesc& desc = texture_case.desc;
        const auto texture = make_texture(*device, desc);
        ASSERT_NE(texture, nullptr) << static_cast<int>(desc.type);
        id<MTLTexture> metal = resources::metal_texture(*texture);
        EXPECT_NE(metal.heap, nil);
        EXPECT_EQ(metal.textureType, texture_case.metal_type);
        EXPECT_EQ(metal.pixelFormat, static_cast<MTLPixelFormat>(desc.format));
        EXPECT_EQ(metal.width, desc.extent.width);
        EXPECT_EQ(metal.height, desc.extent.height);
        EXPECT_EQ(metal.depth, desc.extent.depth);
        EXPECT_EQ(metal.mipmapLevelCount, desc.mip_levels);
        EXPECT_EQ(metal.arrayLength, texture_case.metal_array_length);
        EXPECT_EQ(metal.sampleCount, desc.sample_count);
        EXPECT_EQ(metal.usage, static_cast<MTLTextureUsage>(desc.usage));
        EXPECT_EQ(texture->desc().format, desc.format);
        EXPECT_EQ(texture->desc().extent.width, desc.extent.width);
    }
    EXPECT_EQ(device->memory_stats().dedicated_count, 0u);
}

TEST(Textures, StorageModeSelectsHeap) {
    const auto device = make_device();
    const auto shared = make_texture(*device, texture_2d(rgba8, 16, StorageMode::shared));
    const auto gpu_private = make_texture(*device, texture_2d(rgba8, 16, gpu));
    EXPECT_EQ(resources::metal_texture(*shared).storageMode, MTLStorageModeShared);
    EXPECT_EQ(resources::metal_texture(*gpu_private).storageMode, MTLStorageModePrivate);
    EXPECT_NE(resources::metal_texture(*shared).heap, resources::metal_texture(*gpu_private).heap);
}

TEST(Textures, MemorylessTexturesAreDedicatedAndUseNoBytes) {
    const auto device = make_device();
    const auto texture = make_texture(*device, texture_2d(rgba8, 64, StorageMode::memoryless));
    ASSERT_NE(texture, nullptr);
    id<MTLTexture> metal = resources::metal_texture(*texture);
    EXPECT_EQ(metal.heap, nil);
    EXPECT_EQ(metal.storageMode, MTLStorageModeMemoryless);
    const MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.dedicated_count, 1u);
    EXPECT_EQ(stats.used_bytes, 0u);
    EXPECT_EQ(stats.heap_count, 0u);
    EXPECT_EQ(texture->allocation_size(), 0u);
}

TEST(Textures, ReportTheirAllocationSize) {
    const auto device = make_device();
    const TextureDesc desc = texture_2d(rgba8, 64, gpu);
    const auto texture = make_texture(*device, desc);
    ASSERT_NE(texture, nullptr);
    MTLTextureDescriptor* descriptor = resources::texture_descriptor(desc);
    const MTLSizeAndAlign need =
        [resources::metal_device(*device) heapTextureSizeAndAlignWithDescriptor:descriptor];
    EXPECT_EQ(texture->allocation_size(), need.size);
    EXPECT_GE(texture->allocation_size(), 64u * 64u * 4u);
    EXPECT_EQ(device->memory_stats().used_bytes, texture->allocation_size());
}

TEST(Textures, LargeTexturesAreDedicated) {
    const auto device = make_device({.heap_block_size = 1 * mib, .dedicated_threshold = 1 * mib});
    const auto small = make_texture(*device, texture_2d(rgba8, 256, gpu));
    const auto large = make_texture(*device, texture_2d(rgba8, 1024, gpu));
    EXPECT_NE(resources::metal_texture(*small).heap, nil);
    EXPECT_EQ(resources::metal_texture(*large).heap, nil);
    EXPECT_EQ(device->memory_stats().dedicated_count, 1u);
}

} // namespace
