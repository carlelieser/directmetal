#pragma once

#include "dxgi_row.hpp"

// 8- and 16-bit color formats, DXGI_FORMAT_R8G8_TYPELESS through DXGI_FORMAT_G8R8_G8B8_UNORM.
namespace directmetal::formats::tables {

// Metal's 4:2:2 formats are 2D only and have no mipmaps.
inline constexpr Support packed_422_color =
    Support::texture2d | Support::shader_load | Support::shader_sample;

inline constexpr DxgiRow small_format_rows[] = {
    {48, "DXGI_FORMAT_R8G8_TYPELESS", rg8_unorm, typeless, 48, 2, typeless_color},
    {49, "DXGI_FORMAT_R8G8_UNORM", rg8_unorm, unorm, 48, 2, filterable_color},
    {50, "DXGI_FORMAT_R8G8_UINT", rg8_uint, uint, 48, 2, integer_color},
    {51, "DXGI_FORMAT_R8G8_SNORM", rg8_snorm, snorm, 48, 2, filterable_color},
    {52, "DXGI_FORMAT_R8G8_SINT", rg8_sint, sint, 48, 2, integer_color},
    {53, "DXGI_FORMAT_R16_TYPELESS", r16_unorm, typeless, 53, 2, typeless_color, Substitution::none,
     identity_swizzle, 1, 1, 1, depth16_unorm},
    {54, "DXGI_FORMAT_R16_FLOAT", r16_float, float_value, 53, 2,
     filterable_color | Support::typed_uav_load},
    {56, "DXGI_FORMAT_R16_UNORM", r16_unorm, unorm, 53, 2, filterable_color},
    {57, "DXGI_FORMAT_R16_UINT", r16_uint, uint, 53, 2,
     integer_color | Support::index_buffer | Support::typed_uav_load},
    {58, "DXGI_FORMAT_R16_SNORM", r16_snorm, snorm, 53, 2, filterable_color},
    {59, "DXGI_FORMAT_R16_SINT", r16_sint, sint, 53, 2, integer_color | Support::typed_uav_load},
    {60, "DXGI_FORMAT_R8_TYPELESS", r8_unorm, typeless, 60, 1, typeless_color},
    {61, "DXGI_FORMAT_R8_UNORM", r8_unorm, unorm, 60, 1,
     filterable_color | Support::typed_uav_load},
    {62, "DXGI_FORMAT_R8_UINT", r8_uint, uint, 60, 1, integer_color | Support::typed_uav_load},
    {63, "DXGI_FORMAT_R8_SNORM", r8_snorm, snorm, 60, 1, filterable_color},
    {64, "DXGI_FORMAT_R8_SINT", r8_sint, sint, 60, 1, integer_color | Support::typed_uav_load},
    {65, "DXGI_FORMAT_A8_UNORM", a8_unorm, unorm, 0, 1,
     textures | Support::buffer | sampled | renderable | Support::typed_uav_store |
         Support::mip_autogen},
    {66, "DXGI_FORMAT_R1_UNORM", invalid, unorm, 0, 1, Support::none, Substitution::unsupported,
     identity_swizzle, 8, 1},
    {68, "DXGI_FORMAT_R8G8_B8G8_UNORM", bgrg422, unorm, 0, 4, packed_422_color, Substitution::none,
     packed_422_swizzle, 2, 1},
    {69, "DXGI_FORMAT_G8R8_G8B8_UNORM", gbgr422, unorm, 0, 4, packed_422_color, Substitution::none,
     packed_422_swizzle, 2, 1},
};

} // namespace directmetal::formats::tables
