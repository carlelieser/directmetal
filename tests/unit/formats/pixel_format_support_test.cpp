#include <directmetal/formats/formats.hpp>

#include <gtest/gtest.h>

namespace {

using directmetal::backend::GpuFamily;
using directmetal::backend::PixelFormat;
using namespace directmetal::formats;

TEST(PixelFormatSupport, ReportsColorAttachmentCapabilities) {
    const PixelFormatSupport rgba8 =
        pixel_format_support(PixelFormat::rgba8_unorm, mac_features(GpuFamily::apple7));
    EXPECT_TRUE(rgba8.is_texture);
    EXPECT_TRUE(rgba8.is_renderable);
    EXPECT_TRUE(rgba8.is_blendable);
    EXPECT_TRUE(rgba8.is_multisample);
    EXPECT_TRUE(rgba8.is_resolvable);
    const PixelFormatSupport integer =
        pixel_format_support(PixelFormat::rgba32_uint, mac_features(GpuFamily::apple7));
    EXPECT_TRUE(integer.is_renderable);
    EXPECT_FALSE(integer.is_blendable);
    EXPECT_FALSE(integer.is_filterable);
}

TEST(PixelFormatSupport, LeavesCompressedFormatsUnrenderable) {
    const PixelFormatSupport bc1 =
        pixel_format_support(PixelFormat::bc1_rgba, mac_features(GpuFamily::apple7));
    EXPECT_TRUE(bc1.is_texture);
    EXPECT_FALSE(bc1.is_renderable);
    EXPECT_FALSE(bc1.is_multisample);
}

TEST(PixelFormatSupport, HonorsDeviceFeatures) {
    DeviceFeatures without_bc = mac_features(GpuFamily::apple9);
    without_bc.has_bc_compression = false;
    EXPECT_FALSE(pixel_format_support(PixelFormat::bc7_rgbaunorm, without_bc).is_texture);
    DeviceFeatures apple7 = mac_features(GpuFamily::apple7);
    EXPECT_FALSE(pixel_format_support(PixelFormat::rgba32_float, apple7).is_filterable);
    EXPECT_TRUE(pixel_format_support(PixelFormat::rgba32_float, mac_features(GpuFamily::apple9))
                    .is_filterable);
}

TEST(PixelFormatSupport, ReportsNothingForUnlistedFormats) {
    const PixelFormatSupport depth24 =
        pixel_format_support(PixelFormat::depth24_unorm_stencil8, mac_features(GpuFamily::apple9));
    EXPECT_FALSE(depth24.is_texture);
    EXPECT_FALSE(depth24.is_renderable);
    EXPECT_FALSE(
        pixel_format_support(PixelFormat::invalid, mac_features(GpuFamily::apple9)).is_texture);
}

} // namespace
