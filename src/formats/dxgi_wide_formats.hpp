#pragma once

#include "dxgi_row.hpp"

// Formats with 16- or 32-bit channels, from DXGI_FORMAT_R32G32B32A32_TYPELESS through
// DXGI_FORMAT_R32G32_SINT, and the color formats of the R32 family.
namespace directmetal::formats::tables {

inline constexpr DxgiRow wide_format_rows[] = {
    {1, "DXGI_FORMAT_R32G32B32A32_TYPELESS", rgba32_float, typeless, 1, 16, typeless_color},
    {2, "DXGI_FORMAT_R32G32B32A32_FLOAT", rgba32_float, float_value, 1, 16,
     filterable_color | Support::typed_uav_load},
    {3, "DXGI_FORMAT_R32G32B32A32_UINT", rgba32_uint, uint, 1, 16,
     integer_color | Support::typed_uav_load},
    {4, "DXGI_FORMAT_R32G32B32A32_SINT", rgba32_sint, sint, 1, 16,
     integer_color | Support::typed_uav_load},
    {5, "DXGI_FORMAT_R32G32B32_TYPELESS", rgb32_float, typeless, 5, 12, Support::buffer,
     Substitution::rgb_as_buffer_only},
    {6, "DXGI_FORMAT_R32G32B32_FLOAT", rgb32_float, float_value, 5, 12, rgb_buffer,
     Substitution::rgb_as_buffer_only},
    {7, "DXGI_FORMAT_R32G32B32_UINT", rgb32_uint, uint, 5, 12, rgb_buffer,
     Substitution::rgb_as_buffer_only},
    {8, "DXGI_FORMAT_R32G32B32_SINT", rgb32_sint, sint, 5, 12, rgb_buffer,
     Substitution::rgb_as_buffer_only},
    {9, "DXGI_FORMAT_R16G16B16A16_TYPELESS", rgba16_float, typeless, 9, 8, typeless_color},
    {10, "DXGI_FORMAT_R16G16B16A16_FLOAT", rgba16_float, float_value, 9, 8,
     filterable_color | Support::display | Support::typed_uav_load},
    {11, "DXGI_FORMAT_R16G16B16A16_UNORM", rgba16_unorm, unorm, 9, 8, filterable_color},
    {12, "DXGI_FORMAT_R16G16B16A16_UINT", rgba16_uint, uint, 9, 8,
     integer_color | Support::typed_uav_load},
    {13, "DXGI_FORMAT_R16G16B16A16_SNORM", rgba16_snorm, snorm, 9, 8, filterable_color},
    {14, "DXGI_FORMAT_R16G16B16A16_SINT", rgba16_sint, sint, 9, 8,
     integer_color | Support::typed_uav_load},
    {15, "DXGI_FORMAT_R32G32_TYPELESS", rg32_float, typeless, 15, 8, typeless_color},
    {16, "DXGI_FORMAT_R32G32_FLOAT", rg32_float, float_value, 15, 8, filterable_color},
    {17, "DXGI_FORMAT_R32G32_UINT", rg32_uint, uint, 15, 8, integer_color},
    {18, "DXGI_FORMAT_R32G32_SINT", rg32_sint, sint, 15, 8, integer_color},
    {39, "DXGI_FORMAT_R32_TYPELESS", r32_float, typeless, 39, 4, typeless_color, Substitution::none,
     identity_swizzle, 1, 1, 1, depth32_float},
    {41, "DXGI_FORMAT_R32_FLOAT", r32_float, float_value, 39, 4,
     filterable_color | comparison | Support::typed_uav_load},
    {42, "DXGI_FORMAT_R32_UINT", r32_uint, uint, 39, 4,
     integer_color | Support::index_buffer | Support::typed_uav_load},
    {43, "DXGI_FORMAT_R32_SINT", r32_sint, sint, 39, 4, integer_color | Support::typed_uav_load},
};

} // namespace directmetal::formats::tables
