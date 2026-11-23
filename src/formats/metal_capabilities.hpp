#pragma once

#include <directmetal/backend/pixel_format.hpp>
#include <directmetal/backend/types.hpp>

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
    // The first family with these capabilities; later families have them too.
    backend::GpuFamily since;
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
inline constexpr Capability buffer_read_write = Capability::texture_buffer | Capability::read_write;

using enum backend::PixelFormat;
inline constexpr backend::GpuFamily apple7 = backend::GpuFamily::apple7;
// Apple7 and Apple8 filter and resolve 32-bit float formats only on some iPadOS devices.
inline constexpr backend::GpuFamily apple9 = backend::GpuFamily::apple9;

inline constexpr CapabilityRow capability_rows[] = {
    {a8_unorm, all_color | buffer_read_write, apple7},
    {r8_unorm, all_color | buffer_read_write, apple7},
    {r8_unorm_srgb, all_color, apple7},
    {r8_snorm, all_color | buffer, apple7},
    {r8_uint, integer_color | buffer_read_write, apple7},
    {r8_sint, integer_color | buffer_read_write, apple7},
    {r16_unorm, all_color | buffer, apple7},
    {r16_snorm, all_color | buffer, apple7},
    {r16_uint, integer_color | buffer_read_write, apple7},
    {r16_sint, integer_color | buffer_read_write, apple7},
    {r16_float, all_color | buffer_read_write, apple7},
    {rg8_unorm, all_color | buffer, apple7},
    {rg8_unorm_srgb, all_color, apple7},
    {rg8_snorm, all_color | buffer, apple7},
    {rg8_uint, integer_color | buffer, apple7},
    {rg8_sint, integer_color | buffer, apple7},
    {b5g6r5_unorm, packed_16bit, apple7},
    {a1bgr5_unorm, packed_16bit, apple7},
    {abgr4_unorm, packed_16bit, apple7},
    {bgr5a1_unorm, packed_16bit, apple7},
    {r32_uint, integer_color | buffer_read_write, apple7},
    {r32_sint, integer_color | buffer_read_write, apple7},
    {r32_float, float32_color | buffer_read_write, apple7},
    {r32_float, float32_filtering, apple9},
    {rg16_unorm, all_color | buffer, apple7},
    {rg16_snorm, all_color | buffer, apple7},
    {rg16_uint, integer_color | buffer, apple7},
    {rg16_sint, integer_color | buffer, apple7},
    {rg16_float, all_color | buffer, apple7},
    {rgba8_unorm, all_color | buffer_read_write, apple7},
    {rgba8_unorm_srgb, all_color, apple7},
    {rgba8_snorm, all_color | buffer, apple7},
    {rgba8_uint, integer_color | buffer_read_write, apple7},
    {rgba8_sint, integer_color | buffer_read_write, apple7},
    {bgra8_unorm, all_color | buffer, apple7},
    {bgra8_unorm_srgb, all_color, apple7},
    {rgb10a2_unorm, all_color | buffer, apple7},
    {bgr10a2_unorm, all_color, apple7},
    {rgb10a2_uint, integer_color | buffer, apple7},
    {rg11b10_float, all_color | buffer, apple7},
    {rgb9e5_float, all_color, apple7},
    {rg32_uint, integer_color | buffer, apple7},
    {rg32_sint, integer_color | buffer, apple7},
    {rg32_float, float32_color | buffer, apple7},
    {rg32_float, float32_filtering, apple9},
    {rgba16_unorm, all_color | buffer, apple7},
    {rgba16_snorm, all_color | buffer, apple7},
    {rgba16_uint, integer_color | buffer_read_write, apple7},
    {rgba16_sint, integer_color | buffer_read_write, apple7},
    {rgba16_float, all_color | buffer_read_write, apple7},
    {rgba32_uint, integer_color | buffer_read_write, apple7},
    {rgba32_sint, integer_color | buffer_read_write, apple7},
    {rgba32_float, float32_color | buffer_read_write, apple7},
    {rgba32_float, float32_filtering, apple9},
    // "Varies" for Apple7 and Apple8, which all Macs with those GPUs support.
    {bc1_rgba, filtered_read, apple7},
    {bc1_rgba_srgb, filtered_read, apple7},
    {bc2_rgba, filtered_read, apple7},
    {bc2_rgba_srgb, filtered_read, apple7},
    {bc3_rgba, filtered_read, apple7},
    {bc3_rgba_srgb, filtered_read, apple7},
    {bc4_runorm, filtered_read, apple7},
    {bc4_rsnorm, filtered_read, apple7},
    {bc5_rgunorm, filtered_read, apple7},
    {bc5_rgsnorm, filtered_read, apple7},
    {bc6h_rgbfloat, filtered_read, apple7},
    {bc6h_rgbufloat, filtered_read, apple7},
    {bc7_rgbaunorm, filtered_read, apple7},
    {bc7_rgbaunorm_srgb, filtered_read, apple7},
    {gbgr422, filtered_read, apple7},
    {bgrg422, filtered_read, apple7},
    {depth16_unorm, depth_attachment | Capability::filter, apple7},
    {depth32_float, depth_attachment, apple7},
    {depth32_float, Capability::filter, apple9},
    {stencil8, depth_attachment, apple7},
    {depth32_float_stencil8, depth_attachment, apple7},
    {depth32_float_stencil8, Capability::filter, apple9},
    {x32_stencil8, Capability::texture | Capability::msaa, apple7},
};

} // namespace directmetal::formats::metal
