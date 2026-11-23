#include <directmetal/formats/formats.hpp>

#include <gtest/gtest.h>

namespace {

using namespace directmetal::formats;

struct ViewCase {
    DxgiFormat resource;
    DxgiFormat view;
    bool is_d3d11_compatible;
    bool is_d3d12_relaxed_compatible;
};

// Values are DXGI_FORMAT enumerators.
constexpr ViewCase view_cases[] = {
    {27, 28, true, true},     // R8G8B8A8_TYPELESS -> R8G8B8A8_UNORM
    {27, 29, true, true},     // -> R8G8B8A8_UNORM_SRGB
    {27, 32, true, true},     // -> R8G8B8A8_SINT
    {27, 87, false, false},   // -> B8G8R8A8_UNORM: another family
    {27, 27, false, false},   // a view needs a typed format
    {28, 28, true, true},     // the resource's own format
    {28, 29, false, true},    // R8G8B8A8_UNORM -> R8G8B8A8_UNORM_SRGB
    {28, 30, false, true},    // R8G8B8A8_UNORM -> R8G8B8A8_UINT
    {41, 42, false, true},    // R32_FLOAT -> R32_UINT
    {71, 72, false, true},    // BC1_UNORM -> BC1_UNORM_SRGB
    {70, 72, true, true},     // BC1_TYPELESS -> BC1_UNORM_SRGB
    {70, 74, false, false},   // BC1_TYPELESS -> BC2_UNORM
    {87, 91, false, true},    // B8G8R8A8_UNORM -> B8G8R8A8_UNORM_SRGB
    {87, 88, false, false},   // B8G8R8A8_UNORM -> B8G8R8X8_UNORM: another family
    {44, 45, true, true},     // R24G8_TYPELESS -> D24_UNORM_S8_UINT
    {44, 46, true, true},     // -> R24_UNORM_X8_TYPELESS
    {44, 47, true, true},     // -> X24_TYPELESS_G8_UINT
    {45, 46, false, false},   // D24_UNORM_S8_UINT -> R24_UNORM_X8_TYPELESS: plane views
    {46, 47, false, false},   // depth plane -> stencil plane
    {19, 21, true, true},     // R32G8X24_TYPELESS -> R32_FLOAT_X8X24_TYPELESS
    {39, 40, true, true},     // R32_TYPELESS -> D32_FLOAT
    {39, 41, true, true},     // R32_TYPELESS -> R32_FLOAT
    {40, 41, false, false},   // D32_FLOAT -> R32_FLOAT: depth is not cast
    {55, 56, false, false},   // D16_UNORM -> R16_UNORM
    {26, 26, true, true},     // R11G11B10_FLOAT has no family but views itself
    {26, 41, false, false},   // R11G11B10_FLOAT -> R32_FLOAT
    {24, 89, false, false},   // R10G10B10A2_UNORM -> R10G10B10_XR_BIAS_A2_UNORM
    {103, 103, false, false}, // NV12 is a video format
    {0, 0, false, false},     // UNKNOWN
    {28, 116, false, false},  // 116 is not a DXGI_FORMAT
};

TEST(ViewCompatibility, FollowsTheCastingRules) {
    for (const ViewCase& entry : view_cases) {
        EXPECT_EQ(is_view_compatible(entry.resource, entry.view, CastingRules::d3d11),
                  entry.is_d3d11_compatible)
            << entry.resource << " -> " << entry.view;
        EXPECT_EQ(is_view_compatible(entry.resource, entry.view, CastingRules::d3d12_relaxed),
                  entry.is_d3d12_relaxed_compatible)
            << entry.resource << " -> " << entry.view;
    }
}

TEST(ViewCompatibility, TypelessResourcesViewEveryTypedMemberOfTheirFamily) {
    for (const FormatInfo& view : all_dxgi_formats()) {
        const bool is_family_member =
            view.kind != ComponentKind::typeless && view.typeless_family != 0;
        if (is_family_member) {
            EXPECT_TRUE(is_view_compatible(view.typeless_family, view.format, CastingRules::d3d11))
                << view.name;
        }
    }
}

TEST(ViewCompatibility, D3d11NeverCastsBetweenTypedFormats) {
    for (const FormatInfo& resource : all_dxgi_formats()) {
        if (resource.kind == ComponentKind::typeless) {
            continue;
        }
        for (const FormatInfo& view : all_dxgi_formats()) {
            const bool is_same = resource.format == view.format;
            if (!is_same) {
                EXPECT_FALSE(is_view_compatible(resource.format, view.format, CastingRules::d3d11))
                    << resource.name << " -> " << view.name;
            }
        }
    }
}

} // namespace
