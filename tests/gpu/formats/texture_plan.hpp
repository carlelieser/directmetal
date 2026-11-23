#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/formats/formats.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The textures a format's claimed usages call for, and what each one must read back.
namespace directmetal::formats::gpu_testing {

// What a planned texture proves after its pattern upload.
enum class Check : std::uint8_t {
    // Reads back the uploaded pattern.
    round_trip,
    // Cleared to zero through a render pass.
    color_clear,
    // Depth cleared to one and stencil to stencil_clear_value.
    depth_clear,
};

inline constexpr std::uint8_t stencil_clear_value = 0x5a;

// Whole mip 0 copied slice after slice (cube faces, array slices), each slice_bytes long.
struct CopyLayout {
    backend::Extent3D extent;
    std::uint32_t row_pitch;
    std::uint32_t image_pitch;
    std::uint32_t slice_count;
    std::uint32_t block_bytes;

    std::uint64_t slice_bytes() const { return std::uint64_t{image_pitch} * extent.depth; }
    std::uint64_t total_bytes() const { return slice_bytes() * slice_count; }
};

struct PlannedTexture {
    std::string_view label;
    backend::TextureDesc desc;
    Check check;
    CopyLayout layout;
};

constexpr bool has_any(Support claimed, Support usages) noexcept {
    return (claimed & usages) != Support::none;
}

constexpr bool has_any(backend::TextureUsage usage, backend::TextureUsage flags) noexcept {
    return (usage & flags) != backend::TextureUsage::none;
}

// Metal rejects these for new textures: they only view the stencil of a combined format.
bool is_view_only(backend::PixelFormat pixel_format) noexcept;
bool is_depth_or_stencil(backend::PixelFormat pixel_format) noexcept;

// The pixel format a texture of info is created with: pixel_format, except that a view-only
// stencil format is backed by its typeless family's depth format.
backend::PixelFormat resource_pixel_format(const FormatInfo& info) noexcept;

// The Metal usages the claimed Direct3D usages need; typeless and view-backed formats add
// pixel-format views.
backend::TextureUsage claimed_usage(const FormatInfo& info, Support claimed) noexcept;

// Bytes per texel block in a buffer copy of pixel_format: the depth aspect for combined formats,
// the table's Direct3D layout otherwise.
std::uint32_t copy_block_bytes(const FormatInfo& info, backend::PixelFormat pixel_format) noexcept;

CopyLayout copy_layout(const FormatInfo& info, const backend::TextureDesc& desc);

// One texture per claimed dimension with exactly the claimed usages, plus depth-stencil textures
// for typeless families that name a depth_pixel_format.
std::vector<PlannedTexture> plan_textures(const FormatInfo& info, Support claimed);

// The bytes uploaded before a planned texture's check.
std::vector<std::byte> upload_pattern(const PlannedTexture& planned);
// The bytes a planned texture must read back after its check.
std::vector<std::byte> expected_readback(const PlannedTexture& planned);

// The enumerator name without "DXGI_FORMAT_", for test names.
std::string short_name(const FormatInfo& info);

} // namespace directmetal::formats::gpu_testing
