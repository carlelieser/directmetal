#pragma once

#include <directmetal/backend/pixel_format.hpp>

#include <cstdint>

namespace directmetal::host {

enum class Aspect : std::uint8_t { color = 0x1, depth = 0x2, stencil = 0x4 };

struct PixelLayout {
    backend::PixelFormat format;
    std::uint8_t block_width;
    std::uint8_t block_height;
    std::uint8_t bytes_per_block;
    // Aspect bits.
    std::uint8_t aspects;
};

// Null for formats the host does not accept for textures.
const PixelLayout* find_layout(backend::PixelFormat format) noexcept;

constexpr bool has_aspect(const PixelLayout& layout, Aspect aspect) noexcept {
    return (layout.aspects & static_cast<std::uint8_t>(aspect)) != 0;
}

constexpr bool is_compressed(const PixelLayout& layout) noexcept {
    return layout.block_width > 1 || layout.block_height > 1;
}

} // namespace directmetal::host
