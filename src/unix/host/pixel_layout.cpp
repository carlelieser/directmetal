#include "pixel_layout.hpp"

#include <algorithm>
#include <array>

namespace directmetal::host {
namespace {

using enum backend::PixelFormat;

constexpr std::uint8_t color = static_cast<std::uint8_t>(Aspect::color);
constexpr std::uint8_t depth = static_cast<std::uint8_t>(Aspect::depth);
constexpr std::uint8_t stencil = static_cast<std::uint8_t>(Aspect::stencil);

constexpr PixelLayout pixel(backend::PixelFormat format, std::uint8_t bytes,
                            std::uint8_t aspects = color) noexcept {
    return {format, 1, 1, bytes, aspects};
}

constexpr PixelLayout block(backend::PixelFormat format, std::uint8_t bytes) noexcept {
    return {format, 4, 4, bytes, color};
}

// Ordinary sampled, renderable and BC formats of Apple7 and later GPUs.
constexpr std::array layouts = {
    pixel(a8_unorm, 1),
    pixel(r8_unorm, 1),
    pixel(r8_unorm_srgb, 1),
    pixel(r8_snorm, 1),
    pixel(r8_uint, 1),
    pixel(r8_sint, 1),
    pixel(r16_unorm, 2),
    pixel(r16_snorm, 2),
    pixel(r16_uint, 2),
    pixel(r16_sint, 2),
    pixel(r16_float, 2),
    pixel(rg8_unorm, 2),
    pixel(rg8_unorm_srgb, 2),
    pixel(rg8_snorm, 2),
    pixel(rg8_uint, 2),
    pixel(rg8_sint, 2),
    pixel(b5g6r5_unorm, 2),
    pixel(a1bgr5_unorm, 2),
    pixel(abgr4_unorm, 2),
    pixel(bgr5a1_unorm, 2),
    pixel(r32_uint, 4),
    pixel(r32_sint, 4),
    pixel(r32_float, 4),
    pixel(rg16_unorm, 4),
    pixel(rg16_snorm, 4),
    pixel(rg16_uint, 4),
    pixel(rg16_sint, 4),
    pixel(rg16_float, 4),
    pixel(rgba8_unorm, 4),
    pixel(rgba8_unorm_srgb, 4),
    pixel(rgba8_snorm, 4),
    pixel(rgba8_uint, 4),
    pixel(rgba8_sint, 4),
    pixel(bgra8_unorm, 4),
    pixel(bgra8_unorm_srgb, 4),
    pixel(rgb10a2_unorm, 4),
    pixel(rgb10a2_uint, 4),
    pixel(rg11b10_float, 4),
    pixel(rgb9e5_float, 4),
    pixel(bgr10a2_unorm, 4),
    pixel(bgr10_xr, 4),
    pixel(bgr10_xr_srgb, 4),
    pixel(rg32_uint, 8),
    pixel(rg32_sint, 8),
    pixel(rg32_float, 8),
    pixel(rgba16_unorm, 8),
    pixel(rgba16_snorm, 8),
    pixel(rgba16_uint, 8),
    pixel(rgba16_sint, 8),
    pixel(rgba16_float, 8),
    pixel(bgra10_xr, 8),
    pixel(bgra10_xr_srgb, 8),
    pixel(rgba32_uint, 16),
    pixel(rgba32_sint, 16),
    pixel(rgba32_float, 16),
    block(bc1_rgba, 8),
    block(bc1_rgba_srgb, 8),
    block(bc2_rgba, 16),
    block(bc2_rgba_srgb, 16),
    block(bc3_rgba, 16),
    block(bc3_rgba_srgb, 16),
    block(bc4_runorm, 8),
    block(bc4_rsnorm, 8),
    block(bc5_rgunorm, 16),
    block(bc5_rgsnorm, 16),
    block(bc6h_rgbfloat, 16),
    block(bc6h_rgbufloat, 16),
    block(bc7_rgbaunorm, 16),
    block(bc7_rgbaunorm_srgb, 16),
    pixel(depth16_unorm, 2, depth),
    pixel(depth32_float, 4, depth),
    pixel(stencil8, 1, stencil),
    pixel(depth32_float_stencil8, 8, depth | stencil),
};

} // namespace

const PixelLayout* find_layout(backend::PixelFormat format) noexcept {
    const auto* found = std::find_if(layouts.begin(), layouts.end(), [format](const auto& layout) {
        return layout.format == format;
    });
    return found == layouts.end() ? nullptr : found;
}

} // namespace directmetal::host
