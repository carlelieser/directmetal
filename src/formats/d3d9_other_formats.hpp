#pragma once

#include "d3d9_row.hpp"

// D3DFORMAT depth, buffer and FOURCC formats, and the FOURCC extensions games query: INTZ, DF24
// and DF16 (sampleable depth), RAWZ, ATI1 and ATI2 (BC4 and BC5) and NULL (a render target
// without memory).
namespace directmetal::formats::tables {

inline constexpr Substitution d3d9_depth24 = Substitution::depth24_to_depth32_stencil8;

inline constexpr D3d9FormatInfo d3d9_other_rows[] = {
    {70, "D3DFMT_D16_LOCKABLE", depth16_unorm, no_substitution, identity_swizzle,
     D3d9Usage::depth_stencil, 2, 1, 1},
    // Metal has no 32-bit normalized depth format.
    {71, "D3DFMT_D32", invalid, Substitution::unsupported, identity_swizzle, D3d9Usage::none, 4, 1,
     1},
    {73, "D3DFMT_D15S1", invalid, Substitution::unsupported, identity_swizzle, D3d9Usage::none, 2,
     1, 1},
    {75, "D3DFMT_D24S8", depth32_float_stencil8, d3d9_depth24, identity_swizzle,
     D3d9Usage::depth_stencil, 4, 1, 1},
    {77, "D3DFMT_D24X8", depth32_float_stencil8, d3d9_depth24, identity_swizzle,
     D3d9Usage::depth_stencil, 4, 1, 1},
    {79, "D3DFMT_D24X4S4", invalid, Substitution::unsupported, identity_swizzle, D3d9Usage::none, 4,
     1, 1},
    {80, "D3DFMT_D16", depth16_unorm, no_substitution, identity_swizzle, D3d9Usage::depth_stencil,
     2, 1, 1},
    {82, "D3DFMT_D32F_LOCKABLE", depth32_float, no_substitution, identity_swizzle,
     D3d9Usage::depth_stencil, 4, 1, 1},
    {83, "D3DFMT_D24FS8", depth32_float_stencil8, d3d9_depth24, identity_swizzle,
     D3d9Usage::depth_stencil, 4, 1, 1},
    // Direct3D 9Ex only.
    {84, "D3DFMT_D32_LOCKABLE", invalid, Substitution::unsupported, identity_swizzle,
     D3d9Usage::none, 4, 1, 1},
    {85, "D3DFMT_S8_LOCKABLE", invalid, Substitution::unsupported, identity_swizzle,
     D3d9Usage::none, 1, 1, 1},
    {100, "D3DFMT_VERTEXDATA", invalid, no_substitution, identity_swizzle, D3d9Usage::none, 0, 1,
     1},
    {101, "D3DFMT_INDEX16", invalid, no_substitution, identity_swizzle, D3d9Usage::none, 2, 1, 1},
    {102, "D3DFMT_INDEX32", invalid, no_substitution, identity_swizzle, D3d9Usage::none, 4, 1, 1},
    {199, "D3DFMT_BINARYBUFFER", invalid, no_substitution, identity_swizzle, D3d9Usage::none, 1, 1,
     1},
    // Video surfaces: Metal's 4:2:2 formats do no color conversion, so they are not textures.
    {fourcc("UYVY"), "D3DFMT_UYVY", bgrg422, no_substitution, identity_swizzle, D3d9Usage::none, 4,
     2, 1},
    {fourcc("YUY2"), "D3DFMT_YUY2", gbgr422, no_substitution, identity_swizzle, D3d9Usage::none, 4,
     2, 1},
    {fourcc("RGBG"), "D3DFMT_R8G8_B8G8", bgrg422, no_substitution, packed_422_swizzle,
     d3d9_packed_422, 4, 2, 1},
    {fourcc("GRGB"), "D3DFMT_G8R8_G8B8", gbgr422, no_substitution, packed_422_swizzle,
     d3d9_packed_422, 4, 2, 1},
    // DXT2 and DXT4 differ from DXT3 and DXT5 only in premultiplied alpha.
    {fourcc("DXT1"), "D3DFMT_DXT1", bc1_rgba, no_substitution, identity_swizzle, d3d9_compressed, 8,
     4, 4},
    {fourcc("DXT2"), "D3DFMT_DXT2", bc2_rgba, no_substitution, identity_swizzle, d3d9_compressed,
     16, 4, 4},
    {fourcc("DXT3"), "D3DFMT_DXT3", bc2_rgba, no_substitution, identity_swizzle, d3d9_compressed,
     16, 4, 4},
    {fourcc("DXT4"), "D3DFMT_DXT4", bc3_rgba, no_substitution, identity_swizzle, d3d9_compressed,
     16, 4, 4},
    {fourcc("DXT5"), "D3DFMT_DXT5", bc3_rgba, no_substitution, identity_swizzle, d3d9_compressed,
     16, 4, 4},
    {fourcc("MET1"), "D3DFMT_MULTI2_ARGB8", invalid, Substitution::unsupported, identity_swizzle,
     D3d9Usage::none, 0, 1, 1},
    {fourcc("INTZ"), "INTZ", depth32_float_stencil8, d3d9_depth24, identity_swizzle,
     d3d9_sampled_depth, 4, 1, 1},
    {fourcc("DF24"), "DF24", depth32_float_stencil8, d3d9_depth24, identity_swizzle,
     d3d9_sampled_depth, 4, 1, 1},
    {fourcc("DF16"), "DF16", depth16_unorm, no_substitution, identity_swizzle, d3d9_sampled_depth,
     2, 1, 1},
    // Raw depth reads expose a vendor-specific bit layout.
    {fourcc("RAWZ"), "RAWZ", invalid, Substitution::unsupported, identity_swizzle, D3d9Usage::none,
     4, 1, 1},
    {fourcc("ATI1"), "ATI1", bc4_runorm, no_substitution, identity_swizzle,
     D3d9Usage::texture | D3d9Usage::filter, 8, 4, 4},
    {fourcc("ATI2"), "ATI2", bc5_rgunorm, no_substitution, identity_swizzle,
     D3d9Usage::texture | D3d9Usage::filter, 16, 4, 4},
    {fourcc("NULL"), "NULL", invalid, no_substitution, identity_swizzle, D3d9Usage::render_target,
     0, 1, 1},
};

} // namespace directmetal::formats::tables
