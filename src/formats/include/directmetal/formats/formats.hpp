#pragma once

#include <directmetal/backend/types.hpp>

#ifdef _WIN32
#include <dxgiformat.h>
#else
#include <directx/dxgiformat.h>
#endif

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// DXGI_FORMAT and D3DFORMAT tables: Metal format, substitution, swizzle and per-family support.
// The tables are constexpr data; lookups are lock-free.
namespace directmetal::formats {

enum class Support : std::uint32_t {
    none = 0,
    texture1d = 0x1,
    texture2d = 0x2,
    texture3d = 0x4,
    texture_cube = 0x8,
    buffer = 0x10,
    vertex_buffer = 0x20,
    index_buffer = 0x40,
    shader_load = 0x80,
    shader_sample = 0x100,
    shader_gather = 0x200,
    render_target = 0x400,
    blendable = 0x800,
    depth_stencil = 0x1000,
    multisample_render_target = 0x2000,
    multisample_resolve = 0x4000,
    typed_uav_load = 0x8000,
    typed_uav_store = 0x10000,
    display = 0x20000,
    mip_autogen = 0x40000,
};

constexpr Support operator|(Support left, Support right) noexcept {
    return static_cast<Support>(static_cast<std::uint32_t>(left) |
                                static_cast<std::uint32_t>(right));
}

constexpr Support operator&(Support left, Support right) noexcept {
    return static_cast<Support>(static_cast<std::uint32_t>(left) &
                                static_cast<std::uint32_t>(right));
}

enum class Substitution : std::uint8_t {
    none,
    depth24_to_depth32_stencil8,
    alpha_forced_one,
    rgb_as_buffer_only,
    emulated_unpack,
    unsupported,
};

enum class Swizzle : std::uint8_t { red, green, blue, alpha, zero, one };

enum class ComponentKind : std::uint8_t {
    typeless,
    unorm,
    unorm_srgb,
    snorm,
    uint,
    sint,
    float_value,
    depth,
    depth_stencil,
    compressed,
    video,
};

struct FormatInfo {
    DXGI_FORMAT format;
    std::string_view name; // the enumerator name, e.g. "DXGI_FORMAT_R8G8B8A8_UNORM"
    backend::PixelFormat pixel_format;
    Substitution substitution;
    std::array<Swizzle, 4> swizzle;
    ComponentKind kind;
    DXGI_FORMAT typeless_family;
    std::uint8_t block_width;
    std::uint8_t block_height;
    std::uint8_t bytes_per_block;
    std::uint8_t plane_count;
};

// D3D11 casts within a typeless family only; D3D12 relaxed casting also allows casts between
// fully typed formats of the same family and bit layout.
enum class CastingRules : std::uint8_t { d3d11, d3d12_relaxed };

// Null for values outside the DXGI_FORMAT enum.
const FormatInfo* find(DXGI_FORMAT format) noexcept;
// Every DXGI_FORMAT enumerator except DXGI_FORMAT_FORCE_UINT, in enum order.
std::span<const FormatInfo> all_dxgi_formats() noexcept;
Support support(DXGI_FORMAT format, backend::GpuFamily family) noexcept;
bool is_view_compatible(DXGI_FORMAT resource, DXGI_FORMAT view, CastingRules rules) noexcept;

enum class D3d9Usage : std::uint32_t {
    none = 0,
    texture = 0x1,
    cube_texture = 0x2,
    volume_texture = 0x4,
    render_target = 0x8,
    depth_stencil = 0x10,
    vertex_texture = 0x20,
    filter = 0x40,
    blend = 0x80,
    srgb_read = 0x100,
    srgb_write = 0x200,
    auto_gen_mipmap = 0x400,
};

struct D3d9FormatInfo {
    std::uint32_t value; // D3DFORMAT, including FOURCC values
    std::string_view name;
    backend::PixelFormat pixel_format;
    Substitution substitution;
    std::array<Swizzle, 4> swizzle;
    D3d9Usage usage;
    std::uint8_t bytes_per_block;
    std::uint8_t block_width;
    std::uint8_t block_height;
};

// Null for unknown values. Covers the documented D3DFORMAT values and the FOURCC extensions games
// use: INTZ, DF24, DF16, RAWZ, ATI1, ATI2, NULL.
const D3d9FormatInfo* find_d3d9(std::uint32_t d3dformat) noexcept;
std::span<const D3d9FormatInfo> all_d3d9_formats() noexcept;

} // namespace directmetal::formats
