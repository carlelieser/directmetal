#pragma once

#include <directmetal/backend/pixel_format.hpp>
#include <directmetal/backend/types.hpp>
#include <directmetal/formats/formats.hpp>

#include <cstdint>

// Texture capabilities of each Metal pixel format by Apple GPU family, from Apple's Metal feature
// set tables (https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf), "Texture
// capabilities by pixel format" and "Texture buffer pixel formats". Every format can be read; a
// pixel format without a row cannot be used. Sparse and atomic capabilities are not tracked.
namespace directmetal::formats::metal {

enum class Capability : std::uint16_t {
    none = 0,
    texture = 0x1,
    filter = 0x2,
    write = 0x4,
    // Color, depth or stencil attachment.
    render = 0x8,
    blend = 0x10,
    msaa = 0x20,
    resolve = 0x40,
    // Tier 2 read_write access (MTLDevice.readWriteTextureSupport).
    read_write = 0x80,
    texture_buffer = 0x100,
};

constexpr Capability operator|(Capability left, Capability right) noexcept {
    return static_cast<Capability>(static_cast<std::uint16_t>(left) |
                                   static_cast<std::uint16_t>(right));
}

constexpr bool has_all(Capability capabilities, Capability required) noexcept {
    const auto bits = static_cast<std::uint16_t>(required);
    return (static_cast<std::uint16_t>(capabilities) & bits) == bits;
}

struct CapabilityRow {
    backend::PixelFormat pixel_format;
    Capability capabilities;
    // The device feature the capabilities need; null when every Apple7 and later GPU has them.
    bool DeviceFeatures::* feature = nullptr;
};

// Apple's "All".
inline constexpr Capability all_color = Capability::texture | Capability::filter |
                                        Capability::write | Capability::render | Capability::blend |
                                        Capability::msaa | Capability::resolve;
inline constexpr Capability integer_color =
    Capability::texture | Capability::write | Capability::render | Capability::msaa;
inline constexpr Capability float32_color = integer_color | Capability::blend;
inline constexpr Capability packed_16bit = Capability::texture | Capability::filter |
                                           Capability::render | Capability::blend |
                                           Capability::msaa | Capability::resolve;
inline constexpr Capability float32_filtering = Capability::filter | Capability::resolve;
inline constexpr Capability filtered_read = Capability::texture | Capability::filter;
inline constexpr Capability depth_attachment =
    Capability::texture | Capability::render | Capability::msaa | Capability::resolve;
inline constexpr Capability buffer = Capability::texture_buffer;

using enum backend::PixelFormat;
// Apple's tables grant these from Apple9 and leave them to the device on Apple7 and Apple8.
inline constexpr bool DeviceFeatures::* float32_filtering_feature =
    &DeviceFeatures::has_float32_filtering;
inline constexpr bool DeviceFeatures::* bc_compression = &DeviceFeatures::has_bc_compression;
// Tier 2 read_write access to the formats Apple lists as "All" in the texture buffer table; the
// R32 formats are tier 1, which every Apple GPU has.
inline constexpr bool DeviceFeatures::* read_write_tier2 = &DeviceFeatures::has_read_write_tier2;

inline constexpr CapabilityRow capability_rows[] = {
    {a8_unorm, all_color | buffer},
    {a8_unorm, Capability::read_write, read_write_tier2},
    {r8_unorm, all_color | buffer},
    {r8_unorm, Capability::read_write, read_write_tier2},
    {r8_unorm_srgb, all_color},
    {r8_snorm, all_color | buffer},
    {r8_uint, integer_color | buffer},
    {r8_uint, Capability::read_write, read_write_tier2},
    {r8_sint, integer_color | buffer},
    {r8_sint, Capability::read_write, read_write_tier2},
    {r16_unorm, all_color | buffer},
    {r16_snorm, all_color | buffer},
    {r16_uint, integer_color | buffer},
    {r16_uint, Capability::read_write, read_write_tier2},
    {r16_sint, integer_color | buffer},
    {r16_sint, Capability::read_write, read_write_tier2},
    {r16_float, all_color | buffer},
    {r16_float, Capability::read_write, read_write_tier2},
    {rg8_unorm, all_color | buffer},
    {rg8_unorm_srgb, all_color},
    {rg8_snorm, all_color | buffer},
    {rg8_uint, integer_color | buffer},
    {rg8_sint, integer_color | buffer},
    {b5g6r5_unorm, packed_16bit},
    {a1bgr5_unorm, packed_16bit},
    {abgr4_unorm, packed_16bit},
    {bgr5a1_unorm, packed_16bit},
    {r32_uint, integer_color | buffer},
    {r32_uint, Capability::read_write},
    {r32_sint, integer_color | buffer},
    {r32_sint, Capability::read_write},
    {r32_float, float32_color | buffer},
    {r32_float, Capability::read_write},
    {r32_float, float32_filtering, float32_filtering_feature},
    {rg16_unorm, all_color | buffer},
    {rg16_snorm, all_color | buffer},
    {rg16_uint, integer_color | buffer},
    {rg16_sint, integer_color | buffer},
    {rg16_float, all_color | buffer},
    {rgba8_unorm, all_color | buffer},
    {rgba8_unorm, Capability::read_write, read_write_tier2},
    {rgba8_unorm_srgb, all_color},
    {rgba8_snorm, all_color | buffer},
    {rgba8_uint, integer_color | buffer},
    {rgba8_uint, Capability::read_write, read_write_tier2},
    {rgba8_sint, integer_color | buffer},
    {rgba8_sint, Capability::read_write, read_write_tier2},
    {bgra8_unorm, all_color | buffer},
    {bgra8_unorm_srgb, all_color},
    {rgb10a2_unorm, all_color | buffer},
    {bgr10a2_unorm, all_color},
    {rgb10a2_uint, integer_color | buffer},
    {rg11b10_float, all_color | buffer},
    {rgb9e5_float, all_color},
    {rg32_uint, integer_color | buffer},
    {rg32_sint, integer_color | buffer},
    {rg32_float, float32_color | buffer},
    {rg32_float, float32_filtering, float32_filtering_feature},
    {rgba16_unorm, all_color | buffer},
    {rgba16_snorm, all_color | buffer},
    {rgba16_uint, integer_color | buffer},
    {rgba16_uint, Capability::read_write, read_write_tier2},
    {rgba16_sint, integer_color | buffer},
    {rgba16_sint, Capability::read_write, read_write_tier2},
    {rgba16_float, all_color | buffer},
    {rgba16_float, Capability::read_write, read_write_tier2},
    {rgba32_uint, integer_color | buffer},
    {rgba32_uint, Capability::read_write, read_write_tier2},
    {rgba32_sint, integer_color | buffer},
    {rgba32_sint, Capability::read_write, read_write_tier2},
    {rgba32_float, float32_color | buffer},
    {rgba32_float, Capability::read_write, read_write_tier2},
    {rgba32_float, float32_filtering, float32_filtering_feature},
    // "Varies" for Apple7 and Apple8; every Mac with those GPUs supports them.
    {bc1_rgba, filtered_read, bc_compression},
    {bc1_rgba_srgb, filtered_read, bc_compression},
    {bc2_rgba, filtered_read, bc_compression},
    {bc2_rgba_srgb, filtered_read, bc_compression},
    {bc3_rgba, filtered_read, bc_compression},
    {bc3_rgba_srgb, filtered_read, bc_compression},
    {bc4_runorm, filtered_read, bc_compression},
    {bc4_rsnorm, filtered_read, bc_compression},
    {bc5_rgunorm, filtered_read, bc_compression},
    {bc5_rgsnorm, filtered_read, bc_compression},
    {bc6h_rgbfloat, filtered_read, bc_compression},
    {bc6h_rgbufloat, filtered_read, bc_compression},
    {bc7_rgbaunorm, filtered_read, bc_compression},
    {bc7_rgbaunorm_srgb, filtered_read, bc_compression},
    {gbgr422, filtered_read},
    {bgrg422, filtered_read},
    {depth16_unorm, depth_attachment | Capability::filter},
    {depth32_float, depth_attachment},
    {depth32_float, Capability::filter, float32_filtering_feature},
    {stencil8, depth_attachment},
    {depth32_float_stencil8, depth_attachment},
    {depth32_float_stencil8, Capability::filter, float32_filtering_feature},
    {x32_stencil8, Capability::texture | Capability::msaa},
};

} // namespace directmetal::formats::metal
