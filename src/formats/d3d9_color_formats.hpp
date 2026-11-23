#pragma once

#include "d3d9_row.hpp"

// Unsigned, signed, luminance and float D3DFORMAT color formats.
namespace directmetal::formats::tables {

inline constexpr Substitution unsupported_d3d9 = Substitution::unsupported;

inline constexpr D3d9FormatInfo d3d9_color_rows[] = {
    {0, "D3DFMT_UNKNOWN", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 0, 1, 1},
    {20, "D3DFMT_R8G8B8", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 3, 1, 1},
    {21, "D3DFMT_A8R8G8B8", bgra8_unorm, no_substitution, identity_swizzle, d3d9_target | d3d9_srgb,
     4, 1, 1},
    {22, "D3DFMT_X8R8G8B8", bgra8_unorm, forced_alpha, alpha_one_swizzle, d3d9_target | d3d9_srgb,
     4, 1, 1},
    {23, "D3DFMT_R5G6B5", b5g6r5_unorm, no_substitution, identity_swizzle, d3d9_target, 2, 1, 1},
    {24, "D3DFMT_X1R5G5B5", bgr5a1_unorm, forced_alpha, alpha_one_swizzle, d3d9_target, 2, 1, 1},
    {25, "D3DFMT_A1R5G5B5", bgr5a1_unorm, no_substitution, identity_swizzle, d3d9_target, 2, 1, 1},
    {26, "D3DFMT_A4R4G4B4", abgr4_unorm, no_substitution, b4g4r4a4_swizzle, d3d9_filtered, 2, 1, 1},
    {27, "D3DFMT_R3G3B2", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 1, 1, 1},
    {28, "D3DFMT_A8", a8_unorm, no_substitution, identity_swizzle, d3d9_filtered, 1, 1, 1},
    {29, "D3DFMT_A8R3G3B2", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 2, 1, 1},
    {30, "D3DFMT_X4R4G4B4", abgr4_unorm, forced_alpha, x4r4g4b4_swizzle, d3d9_filtered, 2, 1, 1},
    {31, "D3DFMT_A2B10G10R10", rgb10a2_unorm, no_substitution, identity_swizzle, d3d9_target, 4, 1,
     1},
    {32, "D3DFMT_A8B8G8R8", rgba8_unorm, no_substitution, identity_swizzle, d3d9_target, 4, 1, 1},
    {33, "D3DFMT_X8B8G8R8", rgba8_unorm, forced_alpha, alpha_one_swizzle, d3d9_target, 4, 1, 1},
    {34, "D3DFMT_G16R16", rg16_unorm, no_substitution, red_green_swizzle, d3d9_target, 4, 1, 1},
    {35, "D3DFMT_A2R10G10B10", bgr10a2_unorm, no_substitution, identity_swizzle, d3d9_target, 4, 1,
     1},
    {36, "D3DFMT_A16B16G16R16", rgba16_unorm, no_substitution, identity_swizzle, d3d9_target, 8, 1,
     1},
    {40, "D3DFMT_A8P8", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 2, 1, 1},
    {41, "D3DFMT_P8", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 1, 1, 1},
    {50, "D3DFMT_L8", r8_unorm, no_substitution, luminance_swizzle, d3d9_filtered, 1, 1, 1},
    {51, "D3DFMT_A8L8", rg8_unorm, no_substitution, luminance_alpha_swizzle, d3d9_filtered, 2, 1,
     1},
    // Uploads expand each 4:4 texel to the 8:8 layout of D3DFMT_A8L8.
    {52, "D3DFMT_A4L4", rg8_unorm, Substitution::emulated_unpack, luminance_alpha_swizzle,
     d3d9_filtered, 1, 1, 1},
    {60, "D3DFMT_V8U8", rg8_snorm, no_substitution, red_green_swizzle, d3d9_filtered, 2, 1, 1},
    // Mixed signed and unsigned channels have no Metal format.
    {61, "D3DFMT_L6V5U5", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 2, 1, 1},
    {62, "D3DFMT_X8L8V8U8", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 4, 1, 1},
    {63, "D3DFMT_Q8W8V8U8", rgba8_snorm, no_substitution, identity_swizzle, d3d9_filtered, 4, 1, 1},
    {64, "D3DFMT_V16U16", rg16_snorm, no_substitution, red_green_swizzle, d3d9_filtered, 4, 1, 1},
    {67, "D3DFMT_A2W10V10U10", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 4, 1,
     1},
    {81, "D3DFMT_L16", r16_unorm, no_substitution, luminance_swizzle, d3d9_filtered, 2, 1, 1},
    {110, "D3DFMT_Q16W16V16U16", rgba16_snorm, no_substitution, identity_swizzle, d3d9_filtered, 8,
     1, 1},
    {111, "D3DFMT_R16F", r16_float, no_substitution, red_swizzle, d3d9_target, 2, 1, 1},
    {112, "D3DFMT_G16R16F", rg16_float, no_substitution, red_green_swizzle, d3d9_target, 4, 1, 1},
    {113, "D3DFMT_A16B16G16R16F", rgba16_float, no_substitution, identity_swizzle, d3d9_target, 8,
     1, 1},
    {114, "D3DFMT_R32F", r32_float, no_substitution, red_swizzle, d3d9_float32_target, 4, 1, 1},
    {115, "D3DFMT_G32R32F", rg32_float, no_substitution, red_green_swizzle, d3d9_float32_target, 8,
     1, 1},
    {116, "D3DFMT_A32B32G32R32F", rgba32_float, no_substitution, identity_swizzle,
     d3d9_float32_target, 16, 1, 1},
    // CxV8U8 computes its third channel in the sampler.
    {117, "D3DFMT_CxV8U8", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 2, 1, 1},
    {118, "D3DFMT_A1", invalid, unsupported_d3d9, identity_swizzle, D3d9Usage::none, 1, 8, 1},
    {119, "D3DFMT_A2B10G10R10_XR_BIAS", invalid, unsupported_d3d9, identity_swizzle,
     D3d9Usage::none, 4, 1, 1},
};

} // namespace directmetal::formats::tables
