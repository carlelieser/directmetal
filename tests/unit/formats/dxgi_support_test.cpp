#include <directmetal/formats/formats.hpp>

#include <gtest/gtest.h>

#include <array>
#include <set>
#include <string_view>

namespace {

using directmetal::backend::GpuFamily;
using directmetal::backend::PixelFormat;
using namespace directmetal::formats;

bool has(Support support, Support flags) {
    return (support & flags) == flags;
}

const FormatInfo& info(DxgiFormat format) {
    const FormatInfo* found = find(format);
    EXPECT_NE(found, nullptr) << "format " << format;
    static const FormatInfo missing{};
    return found == nullptr ? missing : *found;
}

std::set<std::string_view> formats_with(Support flag, const DeviceFeatures& features) {
    std::set<std::string_view> names;
    for (const FormatInfo& format : all_dxgi_formats()) {
        if (has(support(format.format, features), flag)) {
            names.insert(format.name);
        }
    }
    return names;
}

constexpr DxgiFormat d24_unorm_s8_uint = 45;
constexpr DxgiFormat r24_unorm_x8_typeless = 46;
constexpr DxgiFormat x24_typeless_g8_uint = 47;
constexpr DxgiFormat b8g8r8x8_unorm = 88;
constexpr DxgiFormat r32g32b32_float = 6;
constexpr DxgiFormat r32_float = 41;
constexpr DxgiFormat d32_float = 40;

TEST(DxgiSubstitutions, Depth24StencilUsesDepth32FloatStencil8) {
    for (const DxgiFormat format : {44u, d24_unorm_s8_uint, r24_unorm_x8_typeless}) {
        EXPECT_EQ(info(format).pixel_format, PixelFormat::depth32_float_stencil8) << format;
        EXPECT_EQ(info(format).substitution, Substitution::depth24_to_depth32_stencil8) << format;
    }
    EXPECT_EQ(info(x24_typeless_g8_uint).pixel_format, PixelFormat::x32_stencil8);
    EXPECT_TRUE(has(support(d24_unorm_s8_uint, GpuFamily::apple7),
                    Support::depth_stencil | Support::multisample_render_target));
    EXPECT_FALSE(has(support(d24_unorm_s8_uint, GpuFamily::apple7), Support::texture3d));
}

TEST(DxgiSubstitutions, DepthBindableFormatsNameTheirDepthPixelFormat) {
    struct Expected {
        DxgiFormat format;
        PixelFormat depth_pixel_format;
    };
    constexpr Expected expected[] = {
        {53, PixelFormat::depth16_unorm},
        {55, PixelFormat::depth16_unorm},
        {39, PixelFormat::depth32_float},
        {40, PixelFormat::depth32_float},
        {44, PixelFormat::depth32_float_stencil8},
        {d24_unorm_s8_uint, PixelFormat::depth32_float_stencil8},
        {19, PixelFormat::depth32_float_stencil8},
        {20, PixelFormat::depth32_float_stencil8},
    };
    std::size_t depth_bindable_count = 0;
    for (const FormatInfo& format : all_dxgi_formats()) {
        depth_bindable_count += format.depth_pixel_format != PixelFormat::invalid ? 1 : 0;
    }
    EXPECT_EQ(depth_bindable_count, std::size(expected));
    for (const Expected& entry : expected) {
        EXPECT_EQ(info(entry.format).depth_pixel_format, entry.depth_pixel_format)
            << info(entry.format).name;
    }
}

TEST(DxgiSubstitutions, StencilViewsReadStencilInGreen) {
    const std::array<Swizzle, 4> expected = {Swizzle::zero, Swizzle::red, Swizzle::zero,
                                             Swizzle::one};
    EXPECT_EQ(info(x24_typeless_g8_uint).swizzle, expected);
    EXPECT_EQ(info(22).swizzle, expected);
}

TEST(DxgiSubstitutions, B8G8R8X8ForcesAlphaToOne) {
    const FormatInfo& format = info(b8g8r8x8_unorm);
    EXPECT_EQ(format.pixel_format, PixelFormat::bgra8_unorm);
    EXPECT_EQ(format.substitution, Substitution::alpha_forced_one);
    const std::array<Swizzle, 4> expected = {Swizzle::red, Swizzle::green, Swizzle::blue,
                                             Swizzle::one};
    EXPECT_EQ(format.swizzle, expected);
    EXPECT_TRUE(has(support(b8g8r8x8_unorm, GpuFamily::apple7),
                    Support::render_target | Support::shader_sample));
    EXPECT_EQ(info(93).substitution, Substitution::alpha_forced_one);
}

TEST(DxgiSubstitutions, R32G32B32IsAVertexFormatOnly) {
    for (const DxgiFormat format : {r32g32b32_float, 7u, 8u}) {
        EXPECT_EQ(info(format).substitution, Substitution::rgb_as_buffer_only) << format;
        EXPECT_EQ(info(format).bytes_per_block, 12) << format;
        for (const GpuFamily family : {GpuFamily::apple7, GpuFamily::apple10}) {
            EXPECT_EQ(support(format, family), Support::vertex_buffer) << format;
        }
    }
    EXPECT_EQ(info(r32g32b32_float).pixel_format, PixelFormat::rgb32_float);
}

TEST(DxgiSupport, Float32FilteringStartsAtApple9) {
    for (const DxgiFormat format : {r32_float, 16u, 2u}) {
        for (const GpuFamily family : {GpuFamily::apple7, GpuFamily::apple8}) {
            const Support supported = support(format, family);
            EXPECT_FALSE(has(supported, Support::shader_sample)) << format;
            EXPECT_FALSE(has(supported, Support::multisample_resolve)) << format;
            EXPECT_TRUE(has(supported, Support::render_target | Support::blendable)) << format;
        }
        for (const GpuFamily family : {GpuFamily::apple9, GpuFamily::apple10}) {
            EXPECT_TRUE(
                has(support(format, family), Support::shader_sample | Support::multisample_resolve))
                << format;
        }
    }
}

TEST(DxgiSupport, DeviceFeaturesGrantFloat32FilteringOnApple7) {
    const DeviceFeatures features{GpuFamily::apple7, true, true, true};
    for (const DxgiFormat format : {r32_float, 16u, 2u}) {
        EXPECT_TRUE(
            has(support(format, features), Support::shader_sample | Support::multisample_resolve))
            << format;
    }
    EXPECT_TRUE(has(support(r24_unorm_x8_typeless, features), Support::shader_sample));
}

TEST(DxgiSupport, DeviceFeaturesGateBcCompression) {
    const DeviceFeatures with_bc = mac_features(GpuFamily::apple7);
    DeviceFeatures without_bc = with_bc;
    without_bc.has_bc_compression = false;
    for (const DxgiFormat format : {71u, 80u, 95u, 98u}) {
        EXPECT_TRUE(has(support(format, with_bc), Support::texture2d | Support::shader_sample))
            << format;
        EXPECT_EQ(support(format, without_bc), Support::none) << format;
    }
}

TEST(DxgiSupport, DeviceFeaturesGateTypedUavLoads) {
    DeviceFeatures features = mac_features(GpuFamily::apple10);
    features.has_read_write_tier2 = false;
    EXPECT_TRUE(formats_with(Support::typed_uav_load, features).empty());
    EXPECT_TRUE(has(support(r32_float, features), Support::typed_uav_store));
}

TEST(DxgiSupport, MacFeaturesFollowTheFamily) {
    EXPECT_FALSE(mac_features(GpuFamily::apple8).has_float32_filtering);
    EXPECT_TRUE(mac_features(GpuFamily::apple9).has_float32_filtering);
    for (const GpuFamily family : {GpuFamily::apple7, GpuFamily::apple10}) {
        EXPECT_EQ(mac_features(family).family, family);
        EXPECT_TRUE(mac_features(family).has_bc_compression);
    }
}

TEST(DxgiSupport, DepthFilteringFollowsTheMetalTables) {
    EXPECT_TRUE(has(support(56, GpuFamily::apple7), Support::shader_sample));
    EXPECT_FALSE(has(support(r24_unorm_x8_typeless, GpuFamily::apple8), Support::shader_sample));
    EXPECT_TRUE(has(support(r24_unorm_x8_typeless, GpuFamily::apple9), Support::shader_sample));
    EXPECT_TRUE(has(support(r24_unorm_x8_typeless, GpuFamily::apple7), Support::shader_gather));
    EXPECT_TRUE(has(support(d32_float, GpuFamily::apple7), Support::depth_stencil));
    EXPECT_FALSE(has(support(d32_float, GpuFamily::apple7), Support::render_target));
}

TEST(DxgiSupport, B8G8R8A8HasNoTypedUavs) {
    // Direct3D 11 disallows typed UAVs of B8G8R8A8_UNORM.
    for (const DxgiFormat format : {87u, 88u, 90u, 91u}) {
        EXPECT_FALSE(has(support(format, GpuFamily::apple10), Support::typed_uav_store)) << format;
    }
}

TEST(DxgiSupport, TypedUavLoadsMatchTheDirect3DSet) {
    const std::set<std::string_view> expected = {
        "DXGI_FORMAT_R32G32B32A32_FLOAT",
        "DXGI_FORMAT_R32G32B32A32_UINT",
        "DXGI_FORMAT_R32G32B32A32_SINT",
        "DXGI_FORMAT_R16G16B16A16_FLOAT",
        "DXGI_FORMAT_R16G16B16A16_UINT",
        "DXGI_FORMAT_R16G16B16A16_SINT",
        "DXGI_FORMAT_R8G8B8A8_UNORM",
        "DXGI_FORMAT_R8G8B8A8_UINT",
        "DXGI_FORMAT_R8G8B8A8_SINT",
        "DXGI_FORMAT_R32_FLOAT",
        "DXGI_FORMAT_R32_UINT",
        "DXGI_FORMAT_R32_SINT",
        "DXGI_FORMAT_R16_FLOAT",
        "DXGI_FORMAT_R16_UINT",
        "DXGI_FORMAT_R16_SINT",
        "DXGI_FORMAT_R8_UNORM",
        "DXGI_FORMAT_R8_UINT",
        "DXGI_FORMAT_R8_SINT",
    };
    EXPECT_EQ(formats_with(Support::typed_uav_load, mac_features(GpuFamily::apple7)), expected);
}

TEST(DxgiSupport, OnlyR16AndR32UintAreIndexFormats) {
    const std::set<std::string_view> expected = {"DXGI_FORMAT_R16_UINT", "DXGI_FORMAT_R32_UINT"};
    EXPECT_EQ(formats_with(Support::index_buffer, mac_features(GpuFamily::apple7)), expected);
}

TEST(DxgiSupport, DisplayFormatsAreTheScanoutFormats) {
    const std::set<std::string_view> expected = {
        "DXGI_FORMAT_R8G8B8A8_UNORM",    "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",
        "DXGI_FORMAT_B8G8R8A8_UNORM",    "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB",
        "DXGI_FORMAT_R10G10B10A2_UNORM", "DXGI_FORMAT_R16G16B16A16_FLOAT",
    };
    EXPECT_EQ(formats_with(Support::display, mac_features(GpuFamily::apple7)), expected);
}

TEST(DxgiSupport, CompressedFormatsAreSampledNotRendered) {
    for (DxgiFormat format = 71; format <= 99; ++format) {
        const FormatInfo* found = find(format);
        const bool is_compressed_view = found != nullptr && found->block_width == 4;
        if (!is_compressed_view || found->kind == ComponentKind::typeless) {
            continue;
        }
        const Support supported = support(format, GpuFamily::apple7);
        EXPECT_TRUE(has(supported, Support::texture2d | Support::shader_sample)) << found->name;
        EXPECT_FALSE(has(supported, Support::render_target)) << found->name;
        EXPECT_FALSE(has(supported, Support::texture1d)) << found->name;
    }
}

TEST(DxgiLayout, CompressedKindsKeepTheirComponentType) {
    struct Expected {
        DxgiFormat format;
        ComponentKind kind;
    };
    constexpr Expected expected[] = {
        {71, ComponentKind::compressed_unorm},  {72, ComponentKind::compressed_unorm_srgb},
        {80, ComponentKind::compressed_unorm},  {81, ComponentKind::compressed_snorm},
        {84, ComponentKind::compressed_snorm},  {95, ComponentKind::compressed_ufloat},
        {96, ComponentKind::compressed_sfloat}, {99, ComponentKind::compressed_unorm_srgb},
    };
    for (const Expected& entry : expected) {
        EXPECT_EQ(info(entry.format).kind, entry.kind) << info(entry.format).name;
    }
    for (const DxgiFormat format : {0u, 189u, 190u}) {
        EXPECT_EQ(info(format).kind, ComponentKind::opaque) << format;
        EXPECT_EQ(info(format).typeless_family, 0u) << format;
    }
}

TEST(DxgiLayout, BlocksMatchTheDxgiDocumentation) {
    struct Expected {
        DxgiFormat format;
        std::uint8_t width;
        std::uint8_t height;
        std::uint8_t bytes;
    };
    constexpr Expected expected[] = {
        {71, 4, 4, 8}, {98, 4, 4, 16}, {80, 4, 4, 8}, {68, 2, 1, 4},
        {66, 8, 1, 1}, {107, 2, 1, 4}, {2, 1, 1, 16}, {85, 1, 1, 2},
    };
    for (const Expected& entry : expected) {
        const FormatInfo& format = info(entry.format);
        EXPECT_EQ(format.block_width, entry.width) << format.name;
        EXPECT_EQ(format.block_height, entry.height) << format.name;
        EXPECT_EQ(format.bytes_per_block, entry.bytes) << format.name;
    }
    EXPECT_EQ(info(d24_unorm_s8_uint).plane_count, 2);
    EXPECT_EQ(info(103).plane_count, 2);
    EXPECT_EQ(info(28).plane_count, 1);
}

TEST(DxgiLayout, B4G4R4A4ReordersMetalAbgr4) {
    const FormatInfo& format = info(115);
    EXPECT_EQ(format.pixel_format, PixelFormat::abgr4_unorm);
    const std::array<Swizzle, 4> expected = {Swizzle::green, Swizzle::blue, Swizzle::alpha,
                                             Swizzle::red};
    EXPECT_EQ(format.swizzle, expected);
    EXPECT_FALSE(has(support(115, GpuFamily::apple7), Support::render_target));
    EXPECT_EQ(info(191).pixel_format, PixelFormat::abgr4_unorm);
    EXPECT_TRUE(has(support(191, GpuFamily::apple7), Support::render_target));
}

} // namespace
