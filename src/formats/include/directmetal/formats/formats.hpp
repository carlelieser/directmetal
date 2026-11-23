#pragma once

#include <directmetal/backend/types.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// DXGI_FORMAT and D3DFORMAT tables: Metal format, substitution, swizzle and per-family support.
// The tables are constexpr data; lookups are lock-free. Formats are keyed on raw values so this
// header needs no Windows headers: front ends pass static_cast<std::uint32_t>(DXGI_FORMAT). The
// tables cover every DXGI_FORMAT enumerator in DirectX-Headers, through
// DXGI_FORMAT_A4B4G4R4_UNORM; the table sources check their values against DirectX-Headers'
// dxgiformat.h in native builds.
namespace directmetal::formats {

// A DXGI_FORMAT value.
using DxgiFormat = std::uint32_t;

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
    sample_comparison = 0x80000,
    gather_comparison = 0x100000,
};

constexpr Support operator|(Support left, Support right) noexcept {
    return static_cast<Support>(static_cast<std::uint32_t>(left) |
                                static_cast<std::uint32_t>(right));
}

constexpr Support operator&(Support left, Support right) noexcept {
    return static_cast<Support>(static_cast<std::uint32_t>(left) &
                                static_cast<std::uint32_t>(right));
}

// What the backend does beyond creating pixel_format.
enum class Substitution : std::uint8_t {
    none,
    // The resource is Depth32Float_Stencil8 while Direct3D sees a 24-bit layout: bytes_per_block
    // is the Direct3D layout, so uploads and readbacks convert between the two, and depth bias
    // given in 24-bit units is rescaled for the float format.
    depth24_to_depth32_stencil8,
    // The format has no alpha but pixel_format stores one, which must read as one: views apply the
    // swizzle, and the backend initializes alpha to one, masks alpha out of render target writes
    // and rewrites DEST_ALPHA and INV_DEST_ALPHA blend factors to ONE and ZERO. Applies to
    // B8G8R8X8, D3DFMT_X8R8G8B8, D3DFMT_X1R5G5B5 and their relatives.
    alpha_forced_one,
    // Vertex fetch only: Metal has no texture or texture buffer format for it before Apple11.
    vertex_fetch_only,
    // Uploads unpack the data into pixel_format's layout.
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
    compressed_unorm,
    compressed_unorm_srgb,
    compressed_snorm,
    compressed_ufloat,
    compressed_sfloat,
    video,
    // No component layout: DXGI_FORMAT_UNKNOWN and sampler feedback maps.
    opaque,
};

struct FormatInfo {
    DxgiFormat format;
    std::string_view name; // the enumerator name, e.g. "DXGI_FORMAT_R8G8B8A8_UNORM"
    backend::PixelFormat pixel_format;
    // The pixel format of a resource bound as a depth-stencil target: Depth16Unorm, Depth32Float or
    // Depth32Float_Stencil8 for the depth formats and their typeless families; invalid otherwise.
    backend::PixelFormat depth_pixel_format;
    Substitution substitution;
    std::array<Swizzle, 4> swizzle;
    ComponentKind kind;
    DxgiFormat typeless_family;
    std::uint8_t block_width;
    std::uint8_t block_height;
    // The Direct3D layout, which differs from pixel_format's under depth24_to_depth32_stencil8.
    std::uint8_t bytes_per_block;
    std::uint8_t plane_count;
};

// D3D11 casts from a typeless resource to a member of its family only. D3D12 relaxed casting also
// casts between fully typed members of a family, except float to non-float and SNORM to UNORM
// (https://microsoft.github.io/DirectX-Specs/d3d/RelaxedCasting.html). Depth formats and the
// plane views of depth-stencil families are never cast: Metal cannot view depth as color.
enum class CastingRules : std::uint8_t { d3d11, d3d12_relaxed };

// Capabilities that Apple's feature set tables leave to the device within a family:
// MTLDevice.supports32BitFloatFiltering, supportsBCTextureCompression and readWriteTextureSupport.
struct DeviceFeatures {
    backend::GpuFamily family;
    // Filtering and resolving 32-bit float color formats and Depth32Float; every Apple9 GPU.
    bool has_float32_filtering;
    // Every Apple9 GPU and every Mac with an Apple7 or Apple8 GPU.
    bool has_bc_compression;
    bool has_read_write_tier2;
};

// The features Apple documents for every Mac in the family. Tier 2 read-write textures are
// assumed: Apple's tables leave them to the device.
DeviceFeatures mac_features(backend::GpuFamily family) noexcept;

// Null for values outside the DXGI_FORMAT enum.
const FormatInfo* find(DxgiFormat format) noexcept;
// Every DXGI_FORMAT enumerator except DXGI_FORMAT_FORCE_UINT, in enum order.
std::span<const FormatInfo> all_dxgi_formats() noexcept;
Support support(DxgiFormat format, const DeviceFeatures& features) noexcept;
// support(format, mac_features(family)).
Support support(DxgiFormat format, backend::GpuFamily family) noexcept;
bool is_view_compatible(DxgiFormat resource, DxgiFormat view, CastingRules rules) noexcept;

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

constexpr D3d9Usage operator|(D3d9Usage left, D3d9Usage right) noexcept {
    return static_cast<D3d9Usage>(static_cast<std::uint32_t>(left) |
                                  static_cast<std::uint32_t>(right));
}

constexpr D3d9Usage operator&(D3d9Usage left, D3d9Usage right) noexcept {
    return static_cast<D3d9Usage>(static_cast<std::uint32_t>(left) &
                                  static_cast<std::uint32_t>(right));
}

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
