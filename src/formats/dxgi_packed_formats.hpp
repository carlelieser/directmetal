#pragma once

#include "dxgi_row.hpp"

// 32-bit four- and two-channel formats and the packed 16-bit formats.
namespace directmetal::formats::tables {

inline constexpr DxgiRow packed_format_rows[] = {
    {23, "DXGI_FORMAT_R10G10B10A2_TYPELESS", rgb10a2_unorm, typeless, 23, 4, typeless_color},
    {24, "DXGI_FORMAT_R10G10B10A2_UNORM", rgb10a2_unorm, unorm, 23, 4,
     filterable_color | Support::display},
    // MTLVertexFormat has no unnormalized 10:10:10:2 format.
    {25, "DXGI_FORMAT_R10G10B10A2_UINT", rgb10a2_uint, uint, 23, 4, integer_texel},
    {26, "DXGI_FORMAT_R11G11B10_FLOAT", rg11b10_float, float_value, 0, 4, filterable_color},
    {27, "DXGI_FORMAT_R8G8B8A8_TYPELESS", rgba8_unorm, typeless, 27, 4, typeless_color},
    {28, "DXGI_FORMAT_R8G8B8A8_UNORM", rgba8_unorm, unorm, 27, 4,
     filterable_color | Support::display | Support::typed_uav_load},
    {29, "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB", rgba8_unorm_srgb, unorm_srgb, 27, 4,
     srgb_color | Support::display},
    {30, "DXGI_FORMAT_R8G8B8A8_UINT", rgba8_uint, uint, 27, 4,
     integer_color | Support::typed_uav_load},
    {31, "DXGI_FORMAT_R8G8B8A8_SNORM", rgba8_snorm, snorm, 27, 4, filterable_color},
    {32, "DXGI_FORMAT_R8G8B8A8_SINT", rgba8_sint, sint, 27, 4,
     integer_color | Support::typed_uav_load},
    {33, "DXGI_FORMAT_R16G16_TYPELESS", rg16_float, typeless, 33, 4, typeless_color},
    {34, "DXGI_FORMAT_R16G16_FLOAT", rg16_float, float_value, 33, 4, filterable_color},
    {35, "DXGI_FORMAT_R16G16_UNORM", rg16_unorm, unorm, 33, 4, filterable_color},
    {36, "DXGI_FORMAT_R16G16_UINT", rg16_uint, uint, 33, 4, integer_color},
    {37, "DXGI_FORMAT_R16G16_SNORM", rg16_snorm, snorm, 33, 4, filterable_color},
    {38, "DXGI_FORMAT_R16G16_SINT", rg16_sint, sint, 33, 4, integer_color},
    {67, "DXGI_FORMAT_R9G9B9E5_SHAREDEXP", rgb9e5_float, float_value, 0, 4, textures | sampled},
    {85, "DXGI_FORMAT_B5G6R5_UNORM", b5g6r5_unorm, unorm, 0, 2, packed_16bit_color},
    {86, "DXGI_FORMAT_B5G5R5A1_UNORM", bgr5a1_unorm, unorm, 0, 2, packed_16bit_color},
    {87, "DXGI_FORMAT_B8G8R8A8_UNORM", bgra8_unorm, unorm, 90, 4,
     filterable_color | Support::display},
    // Render targets write the unused byte; reads replace it with one.
    {88, "DXGI_FORMAT_B8G8R8X8_UNORM", bgra8_unorm, unorm, 92, 4,
     textures | sampled | renderable | Support::mip_autogen, Substitution::alpha_forced_one,
     alpha_one_swizzle},
    // Metal's extended-range formats store blue in the low bits and use a different bias.
    {89, "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM", invalid, unorm, 0, 4, Support::none,
     Substitution::unsupported},
    {90, "DXGI_FORMAT_B8G8R8A8_TYPELESS", bgra8_unorm, typeless, 90, 4, textures},
    {91, "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB", bgra8_unorm_srgb, unorm_srgb, 90, 4,
     srgb_color | Support::display},
    {92, "DXGI_FORMAT_B8G8R8X8_TYPELESS", bgra8_unorm, typeless, 92, 4, textures},
    {93, "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB", bgra8_unorm_srgb, unorm_srgb, 92, 4, srgb_color,
     Substitution::alpha_forced_one, alpha_one_swizzle},
    {115, "DXGI_FORMAT_B4G4R4A4_UNORM", abgr4_unorm, unorm, 0, 2, textures | sampled,
     Substitution::none, b4g4r4a4_swizzle},
    {191, "DXGI_FORMAT_A4B4G4R4_UNORM", abgr4_unorm, unorm, 0, 2, packed_16bit_color},
};

} // namespace directmetal::formats::tables
