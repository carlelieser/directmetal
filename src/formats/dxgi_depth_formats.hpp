#pragma once

#include "dxgi_row.hpp"

// Depth and stencil formats and the views of their planes.
namespace directmetal::formats::tables {

// Direct3D returns stencil in the green channel; Metal's stencil views return it in red.
inline constexpr std::array<Swizzle, 4> stencil_swizzle = {Swizzle::zero, Swizzle::red,
                                                           Swizzle::zero, Swizzle::one};
// Apple GPUs have no 24-bit depth formats.
inline constexpr Substitution depth24 = Substitution::depth24_to_depth32_stencil8;

inline constexpr DxgiRow depth_format_rows[] = {
    {19, "DXGI_FORMAT_R32G8X24_TYPELESS", depth32_float_stencil8, typeless, 19, 8, depth_textures,
     Substitution::none, identity_swizzle, 1, 1, 2},
    {20, "DXGI_FORMAT_D32_FLOAT_S8X24_UINT", depth32_float_stencil8, depth_stencil, 19, 8,
     depth_stencil_target, Substitution::none, identity_swizzle, 1, 1, 2},
    {21, "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS", depth32_float_stencil8, float_value, 19, 8,
     depth_read, Substitution::none, identity_swizzle, 1, 1, 2},
    {22, "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT", x32_stencil8, uint, 19, 8, stencil_read,
     Substitution::none, stencil_swizzle, 1, 1, 2},
    {40, "DXGI_FORMAT_D32_FLOAT", depth32_float, depth, 39, 4, depth_stencil_target},
    {44, "DXGI_FORMAT_R24G8_TYPELESS", depth32_float_stencil8, typeless, 44, 4, depth_textures,
     depth24, identity_swizzle, 1, 1, 2},
    {45, "DXGI_FORMAT_D24_UNORM_S8_UINT", depth32_float_stencil8, depth_stencil, 44, 4,
     depth_stencil_target, depth24, identity_swizzle, 1, 1, 2},
    {46, "DXGI_FORMAT_R24_UNORM_X8_TYPELESS", depth32_float_stencil8, unorm, 44, 4, depth_read,
     depth24, identity_swizzle, 1, 1, 2},
    {47, "DXGI_FORMAT_X24_TYPELESS_G8_UINT", x32_stencil8, uint, 44, 4, stencil_read, depth24,
     stencil_swizzle, 1, 1, 2},
    {55, "DXGI_FORMAT_D16_UNORM", depth16_unorm, depth, 53, 2, depth_stencil_target},
};

} // namespace directmetal::formats::tables
