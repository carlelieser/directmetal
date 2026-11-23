#pragma once

#include "dxgi_row.hpp"

// Block-compressed formats (BC1 through BC7), 4x4 texel blocks.
namespace directmetal::formats::tables {

inline constexpr DxgiRow compressed_format_rows[] = {
    {70, "DXGI_FORMAT_BC1_TYPELESS", bc1_rgba, typeless, 70, 8, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {71, "DXGI_FORMAT_BC1_UNORM", bc1_rgba, compressed_unorm, 70, 8, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {72, "DXGI_FORMAT_BC1_UNORM_SRGB", bc1_rgba_srgb, compressed_unorm_srgb, 70, 8,
     compressed_color, Substitution::none, identity_swizzle, 4, 4},
    {73, "DXGI_FORMAT_BC2_TYPELESS", bc2_rgba, typeless, 73, 16, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {74, "DXGI_FORMAT_BC2_UNORM", bc2_rgba, compressed_unorm, 73, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {75, "DXGI_FORMAT_BC2_UNORM_SRGB", bc2_rgba_srgb, compressed_unorm_srgb, 73, 16,
     compressed_color, Substitution::none, identity_swizzle, 4, 4},
    {76, "DXGI_FORMAT_BC3_TYPELESS", bc3_rgba, typeless, 76, 16, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {77, "DXGI_FORMAT_BC3_UNORM", bc3_rgba, compressed_unorm, 76, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {78, "DXGI_FORMAT_BC3_UNORM_SRGB", bc3_rgba_srgb, compressed_unorm_srgb, 76, 16,
     compressed_color, Substitution::none, identity_swizzle, 4, 4},
    {79, "DXGI_FORMAT_BC4_TYPELESS", bc4_runorm, typeless, 79, 8, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {80, "DXGI_FORMAT_BC4_UNORM", bc4_runorm, compressed_unorm, 79, 8, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {81, "DXGI_FORMAT_BC4_SNORM", bc4_rsnorm, compressed_snorm, 79, 8, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {82, "DXGI_FORMAT_BC5_TYPELESS", bc5_rgunorm, typeless, 82, 16, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {83, "DXGI_FORMAT_BC5_UNORM", bc5_rgunorm, compressed_unorm, 82, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {84, "DXGI_FORMAT_BC5_SNORM", bc5_rgsnorm, compressed_snorm, 82, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {94, "DXGI_FORMAT_BC6H_TYPELESS", bc6h_rgbufloat, typeless, 94, 16, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {95, "DXGI_FORMAT_BC6H_UF16", bc6h_rgbufloat, compressed_ufloat, 94, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {96, "DXGI_FORMAT_BC6H_SF16", bc6h_rgbfloat, compressed_sfloat, 94, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {97, "DXGI_FORMAT_BC7_TYPELESS", bc7_rgbaunorm, typeless, 97, 16, compressed_typeless,
     Substitution::none, identity_swizzle, 4, 4},
    {98, "DXGI_FORMAT_BC7_UNORM", bc7_rgbaunorm, compressed_unorm, 97, 16, compressed_color,
     Substitution::none, identity_swizzle, 4, 4},
    {99, "DXGI_FORMAT_BC7_UNORM_SRGB", bc7_rgbaunorm_srgb, compressed_unorm_srgb, 97, 16,
     compressed_color, Substitution::none, identity_swizzle, 4, 4},
};

} // namespace directmetal::formats::tables
