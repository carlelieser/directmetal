#include "format_fixture.hpp"

#include <algorithm>
#include <string_view>

// The other direction: usages Metal would reject stay unclaimed. Each is checked against the
// documented capabilities of the running device, never by issuing the call Metal would reject.
namespace directmetal::formats::gpu_testing {
namespace {

constexpr Support any_dimension =
    Support::texture1d | Support::texture2d | Support::texture3d | Support::texture_cube;

struct UnclaimedCase {
    DxgiFormat format;
    Support usage;
    std::string_view reason;
};

constexpr UnclaimedCase unclaimed_cases[] = {
    {6, any_dimension, "RGB32 pixel formats need Apple11"},
    {71, Support::render_target, "compressed formats are not renderable"},
    {40, Support::texture3d, "Metal has no 3D depth textures"},
    {55, Support::texture1d, "Metal has no 1D depth textures"},
    {25, Support::vertex_buffer, "MTLVertexFormat has no unnormalized 10:10:10:2 format"},
    {115, Support::render_target, "its swizzle moves stored channels and applies to reads only"},
    {32, Support::multisample_resolve, "integer formats do not resolve"},
    {3, Support::shader_sample, "integer formats are not filterable"},
    {68, Support::texture3d | Support::texture_cube, "4:2:2 formats are 2D only"},
};

class UnclaimedUsageTest : public FormatGpuTest {};

TEST_F(UnclaimedUsageTest, DocumentedRejectionsStayUnclaimed) {
    for (const UnclaimedCase& entry : unclaimed_cases) {
        const FormatInfo* info = find(entry.format);
        ASSERT_NE(info, nullptr) << entry.format;
        EXPECT_FALSE(has_any(support(entry.format, features()), entry.usage))
            << info->name << ": " << entry.reason;
    }
}

TEST_F(UnclaimedUsageTest, RgbThirtyTwoIsVertexFetchOnlyBeforeApple11) {
    ASSERT_FALSE([metal_device() supportsFamily:MTLGPUFamilyApple11])
        << "Apple11 has RGB32 texture buffers; the table does not claim them yet";
    for (const DxgiFormat format : {DxgiFormat{6}, DxgiFormat{7}, DxgiFormat{8}}) {
        EXPECT_EQ(find(format)->substitution, Substitution::vertex_fetch_only);
        EXPECT_EQ(support(format, features()), Support::vertex_buffer) << find(format)->name;
    }
}

TEST_F(UnclaimedUsageTest, CapabilitiesMatchDevice) {
    const backend::Capabilities& capabilities = device().capabilities();
    EXPECT_EQ(capabilities.has_bc != 0,
              static_cast<bool>(metal_device().supportsBCTextureCompression));
    const auto next_family =
        static_cast<int>(MTLGPUFamilyApple7) + static_cast<int>(capabilities.family) + 1;
    EXPECT_FALSE([metal_device() supportsFamily:static_cast<MTLGPUFamily>(next_family)]);
}

// mac_features is what every Mac in the family has, so the device claims at least as much.
TEST_F(UnclaimedUsageTest, MacFeaturesClaimNoMoreThanDevice) {
    const DeviceFeatures family_features = mac_features(features().family);
    for (const FormatInfo& info : all_dxgi_formats()) {
        const Support documented = support(info.format, family_features);
        const Support actual = support(info.format, features());
        EXPECT_EQ(documented & actual, documented) << info.name;
    }
}

TEST_F(UnclaimedUsageTest, ReadWriteTierOneLoadsOnlyRThirtyTwo) {
    DeviceFeatures tier_one = features();
    tier_one.has_read_write_tier2 = false;
    for (const FormatInfo& info : all_dxgi_formats()) {
        const bool is_r32 = info.format >= 41 && info.format <= 43;
        const bool is_claimed = has_any(support(info.format, tier_one), Support::typed_uav_load);
        EXPECT_TRUE(!is_claimed || is_r32) << info.name;
    }
    const bool is_tier2 = metal_device().readWriteTextureSupport == MTLReadWriteTextureTier2;
    EXPECT_EQ(features().has_read_write_tier2, is_tier2);
}

TEST_F(UnclaimedUsageTest, NoBcClaimsWithoutBcSupport) {
    DeviceFeatures without_bc = features();
    without_bc.has_bc_compression = false;
    for (const FormatInfo& info : all_dxgi_formats()) {
        const bool is_bc = info.block_width == 4;
        EXPECT_TRUE(!is_bc || support(info.format, without_bc) == Support::none) << info.name;
    }
}

TEST_F(UnclaimedUsageTest, FloatThirtyTwoFilteringFollowsDevice) {
    DeviceFeatures unfiltered = features();
    unfiltered.has_float32_filtering = false;
    for (const DxgiFormat format : {DxgiFormat{2}, DxgiFormat{16}, DxgiFormat{41}}) {
        const Support claimed = support(format, unfiltered);
        EXPECT_FALSE(has_any(claimed, Support::shader_sample | Support::multisample_resolve))
            << find(format)->name;
        EXPECT_TRUE(has_any(claimed, Support::render_target)) << find(format)->name;
    }
    EXPECT_EQ(features().has_float32_filtering,
              static_cast<bool>(metal_device().supports32BitFloatFiltering));
}

// Known table gap: these rows claim textures, but X32_Stencil8 cannot back a texture and the rows
// name no depth_pixel_format, so resource_pixel_format falls back to their family's.
TEST(KnownMismatches, ViewOnlyRowsNameNoResourceFormat) {
    std::vector<DxgiFormat> view_only;
    for (const FormatInfo& info : all_dxgi_formats()) {
        if (is_view_only(info.pixel_format)) {
            EXPECT_EQ(info.depth_pixel_format, backend::PixelFormat::invalid) << info.name;
            view_only.push_back(info.format);
        }
    }
    EXPECT_EQ(view_only, (std::vector<DxgiFormat>{22, 47}));
}

} // namespace
} // namespace directmetal::formats::gpu_testing
