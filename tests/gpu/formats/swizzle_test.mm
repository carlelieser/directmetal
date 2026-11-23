#include "format_fixture.hpp"

#include "resource_access.hpp"

#include <algorithm>
#include <array>
#include <cstring>

// Every format whose table swizzle is not the identity is viewed with that swizzle and read by a
// one-thread compute kernel, which must return the channels Direct3D defines.
namespace directmetal::formats::gpu_testing {
namespace {

using backend::TextureUsage;

constexpr char kernel_source[] = R"(
#include <metal_stdlib>
using namespace metal;
kernel void read_float(texture2d<float, access::read> source [[texture(0)]],
                       device float4* texel [[buffer(0)]]) {
    texel[0] = source.read(uint2(0, 0));
}
kernel void read_uint(texture2d<uint, access::read> source [[texture(0)]],
                      device float4* texel [[buffer(0)]]) {
    texel[0] = float4(source.read(uint2(0, 0)));
}
)";

constexpr float tolerance = 0.01f;
constexpr backend::Extent3D extent{8, 8, 1};

struct SwizzleCase {
    DxgiFormat format;
    // The first texel block as stored; empty for stencil, which is cleared to stencil_clear_value.
    std::vector<std::uint8_t> block;
    // RGBA as Direct3D reads texel (0, 0).
    std::array<float, 4> expected;
};

void PrintTo(const SwizzleCase& entry, std::ostream* stream) {
    *stream << find(entry.format)->name;
}

std::vector<SwizzleCase> swizzle_cases() {
    constexpr float stencil = stencil_clear_value;
    return {
        {22, {}, {0, stencil, 0, 1}},
        {47, {}, {0, stencil, 0, 1}},
        // R8G8_B8G8 stores R, G0, B, G1; G8R8_G8B8 stores G0, R, G1, B.
        {68, {0x10, 0x20, 0x30, 0x40}, {0x10 / 255.0f, 0x20 / 255.0f, 0x30 / 255.0f, 1}},
        {69, {0x20, 0x10, 0x21, 0x30}, {0x10 / 255.0f, 0x20 / 255.0f, 0x30 / 255.0f, 1}},
        // B, G, R and an unused byte that must read as one.
        {88, {0x40, 0x80, 0xc0, 0x00}, {0xc0 / 255.0f, 0x80 / 255.0f, 0x40 / 255.0f, 1}},
        // sRGB 0xc0, 0x80 and 0x40 decode to 0.527, 0.216 and 0.0513.
        {93, {0x40, 0x80, 0xc0, 0x00}, {0.527f, 0.216f, 0.0513f, 1}},
        // 0x4321: B in bits 0-3, then G, R and A.
        {115, {0x21, 0x43}, {3 / 15.0f, 2 / 15.0f, 1 / 15.0f, 4 / 15.0f}},
    };
}

constexpr std::array<MTLTextureSwizzle, 6> metal_swizzles{
    MTLTextureSwizzleRed,   MTLTextureSwizzleGreen, MTLTextureSwizzleBlue,
    MTLTextureSwizzleAlpha, MTLTextureSwizzleZero,  MTLTextureSwizzleOne,
};

MTLTextureSwizzleChannels metal_swizzle(const std::array<Swizzle, 4>& swizzle) {
    const auto channel = [&swizzle](std::size_t index) {
        return metal_swizzles[static_cast<std::size_t>(swizzle[index])];
    };
    return MTLTextureSwizzleChannelsMake(channel(0), channel(1), channel(2), channel(3));
}

bool has_identity_swizzle(const FormatInfo& info) {
    return info.swizzle == std::array{Swizzle::red, Swizzle::green, Swizzle::blue, Swizzle::alpha};
}

class SwizzleTest : public FormatGpuTest, public ::testing::WithParamInterface<SwizzleCase> {
protected:
    const FormatInfo& info() const { return *find(GetParam().format); }

    // The stencil of a typeless depth-stencil family is cleared through a depth-stencil binding.
    backend::TextureDesc resource_desc() const {
        TextureUsage usage = claimed_usage(info(), support(info().format, features()));
        if (GetParam().block.empty()) {
            usage = usage | TextureUsage::render_target;
        }
        return {backend::TextureType::tex2d,
                backend::StorageMode::gpu_private,
                0,
                resource_pixel_format(info()),
                extent,
                1,
                1,
                1,
                usage};
    }

    void fill(backend::Texture& texture) {
        const std::vector<std::uint8_t>& block = GetParam().block;
        if (block.empty()) {
            const auto aspects = static_cast<backend::ClearAspect>(0x3);
            queue().recorder().clear_depth_stencil(texture, {0, 1, 0, 1},
                                                   {0.5f, stencil_clear_value, aspects, 0});
            submit_and_wait();
            return;
        }
        const CopyLayout layout = copy_layout(info(), texture.desc());
        std::vector<std::byte> bytes(layout.total_bytes());
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = std::byte{block[index % block.size()]};
        }
        const auto upload = make_buffer(bytes);
        record_upload(*upload, texture, layout);
        submit_and_wait();
    }

    std::array<float, 4> read_texel(id<MTLTexture> view) {
        NSError* error = nil;
        id<MTLLibrary> library = [metal_device() newLibraryWithSource:@(kernel_source)
                                                              options:nil
                                                                error:&error];
        const bool is_uint = info().kind == ComponentKind::uint;
        id<MTLFunction> function =
            [library newFunctionWithName:is_uint ? @"read_uint" : @"read_float"];
        id<MTLComputePipelineState> pipeline =
            [metal_device() newComputePipelineStateWithFunction:function error:&error];
        EXPECT_NE(pipeline, nil) << error.localizedDescription.UTF8String;
        id<MTLBuffer> texel = [metal_device() newBufferWithLength:16
                                                          options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> commands = [[metal_device() newCommandQueue] commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commands computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];
        [encoder setTexture:view atIndex:0];
        [encoder setBuffer:texel offset:0 atIndex:0];
        [encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [encoder endEncoding];
        [commands commit];
        [commands waitUntilCompleted];
        std::array<float, 4> channels{};
        std::memcpy(channels.data(), texel.contents, sizeof(channels));
        return channels;
    }
};

TEST_P(SwizzleTest, ReadsDirect3DChannelsThroughSwizzledView) {
    ASSERT_NE(find(GetParam().format), nullptr);
    if (plan_textures(info(), support(info().format, features())).empty()) {
        GTEST_SKIP() << "no texture dimension claimed on this device";
    }
    const auto texture = make_texture(resource_desc());
    ASSERT_NE(texture, nullptr);
    fill(*texture);
    id<MTLTexture> view = [backend::resources::metal_texture(*texture)
        newTextureViewWithPixelFormat:static_cast<MTLPixelFormat>(info().pixel_format)
                          textureType:MTLTextureType2D
                               levels:NSMakeRange(0, 1)
                               slices:NSMakeRange(0, 1)
                              swizzle:metal_swizzle(info().swizzle)];
    ASSERT_NE(view, nil);
    const std::array<float, 4> texel = read_texel(view);
    for (std::size_t channel = 0; channel < texel.size(); ++channel) {
        EXPECT_NEAR(texel[channel], GetParam().expected[channel], tolerance)
            << "channel " << channel;
    }
}

INSTANTIATE_TEST_SUITE_P(DxgiFormats, SwizzleTest, ::testing::ValuesIn(swizzle_cases()),
                         [](const auto& param) { return short_name(*find(param.param.format)); });

TEST(SwizzleCases, CoverEverySwizzledFormat) {
    const std::vector<SwizzleCase> cases = swizzle_cases();
    for (const FormatInfo& info : all_dxgi_formats()) {
        const bool is_covered = std::ranges::any_of(
            cases, [&info](const SwizzleCase& entry) { return entry.format == info.format; });
        EXPECT_EQ(is_covered, !has_identity_swizzle(info)) << info.name;
    }
}

} // namespace
} // namespace directmetal::formats::gpu_testing
