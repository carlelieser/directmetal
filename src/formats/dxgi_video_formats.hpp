#pragma once

#include "dxgi_row.hpp"

// DXGI_FORMAT_UNKNOWN, video, palettized and opaque formats. None has a Metal pixel format.
// bytes_per_block is the size of a block of the first plane.
namespace directmetal::formats::tables {

inline constexpr Substitution no_metal_format = Substitution::unsupported;

inline constexpr DxgiRow video_format_rows[] = {
    {0, "DXGI_FORMAT_UNKNOWN", invalid, typeless, 0, 0, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 0},
    {100, "DXGI_FORMAT_AYUV", invalid, video, 0, 4, Support::none, no_metal_format},
    {101, "DXGI_FORMAT_Y410", invalid, video, 0, 4, Support::none, no_metal_format},
    {102, "DXGI_FORMAT_Y416", invalid, video, 0, 8, Support::none, no_metal_format},
    {103, "DXGI_FORMAT_NV12", invalid, video, 0, 1, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 2},
    {104, "DXGI_FORMAT_P010", invalid, video, 0, 2, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 2},
    {105, "DXGI_FORMAT_P016", invalid, video, 0, 2, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 2},
    {106, "DXGI_FORMAT_420_OPAQUE", invalid, video, 0, 1, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 2},
    {107, "DXGI_FORMAT_YUY2", invalid, video, 0, 4, Support::none, no_metal_format,
     identity_swizzle, 2, 1},
    {108, "DXGI_FORMAT_Y210", invalid, video, 0, 8, Support::none, no_metal_format,
     identity_swizzle, 2, 1},
    {109, "DXGI_FORMAT_Y216", invalid, video, 0, 8, Support::none, no_metal_format,
     identity_swizzle, 2, 1},
    {110, "DXGI_FORMAT_NV11", invalid, video, 0, 1, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 2},
    {111, "DXGI_FORMAT_AI44", invalid, video, 0, 1, Support::none, no_metal_format},
    {112, "DXGI_FORMAT_IA44", invalid, video, 0, 1, Support::none, no_metal_format},
    {113, "DXGI_FORMAT_P8", invalid, video, 0, 1, Support::none, no_metal_format},
    {114, "DXGI_FORMAT_A8P8", invalid, video, 0, 2, Support::none, no_metal_format},
    {130, "DXGI_FORMAT_P208", invalid, video, 0, 1, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 2},
    {131, "DXGI_FORMAT_V208", invalid, video, 0, 1, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 3},
    {132, "DXGI_FORMAT_V408", invalid, video, 0, 1, Support::none, no_metal_format,
     identity_swizzle, 1, 1, 3},
    // Sampler feedback maps have an opaque layout.
    {189, "DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE", invalid, typeless, 189, 0, Support::none,
     no_metal_format},
    {190, "DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE", invalid, typeless, 190, 0,
     Support::none, no_metal_format},
};

} // namespace directmetal::formats::tables
