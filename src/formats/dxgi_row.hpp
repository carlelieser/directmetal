#pragma once

#include <directmetal/formats/formats.hpp>

#include <array>
#include <cstdint>
#include <string_view>

// One DXGI_FORMAT table row: the public FormatInfo fields plus the usages Direct3D allows for the
// format. support() masks those usages with the Metal capabilities of the row's pixel format.
namespace directmetal::formats::tables {

using enum backend::PixelFormat;
using enum ComponentKind;

inline constexpr std::array<Swizzle, 4> identity_swizzle = {Swizzle::red, Swizzle::green,
                                                            Swizzle::blue, Swizzle::alpha};
inline constexpr std::array<Swizzle, 4> alpha_one_swizzle = {Swizzle::red, Swizzle::green,
                                                             Swizzle::blue, Swizzle::one};
// Swizzles apply only to reads, so formats whose swizzle moves stored channels are not render
// targets.
inline constexpr std::array<Swizzle, 4> b4g4r4a4_swizzle = {Swizzle::green, Swizzle::blue,
                                                            Swizzle::alpha, Swizzle::red};
// Metal's 4:2:2 formats return (byte 2 or 3, green, byte 0 or 1); DXGI names red first.
inline constexpr std::array<Swizzle, 4> packed_422_swizzle = {Swizzle::blue, Swizzle::green,
                                                              Swizzle::red, Swizzle::one};

struct DxgiRow {
    DxgiFormat format;
    std::string_view name;
    backend::PixelFormat pixel_format;
    ComponentKind kind;
    DxgiFormat typeless_family;
    std::uint8_t bytes_per_block;
    Support usage;
    Substitution substitution = Substitution::none;
    std::array<Swizzle, 4> swizzle = identity_swizzle;
    std::uint8_t block_width = 1;
    std::uint8_t block_height = 1;
    std::uint8_t plane_count = 1;
};

// Usage profiles from Microsoft's Direct3D 11 format support tables
// (https://learn.microsoft.com/windows/win32/direct3ddxgi/format-support-for-direct3d-11-0-feature-level-hardware).
// Optional usages are included; Metal's capabilities decide whether they are reported.
inline constexpr Support textures =
    Support::texture1d | Support::texture2d | Support::texture3d | Support::texture_cube;
inline constexpr Support sampled =
    Support::shader_load | Support::shader_sample | Support::shader_gather;
inline constexpr Support renderable = Support::render_target | Support::blendable |
                                      Support::multisample_render_target |
                                      Support::multisample_resolve;
inline constexpr Support filterable_color = textures | Support::buffer | Support::vertex_buffer |
                                            sampled | renderable | Support::typed_uav_store |
                                            Support::mip_autogen;
inline constexpr Support srgb_color = textures | sampled | renderable | Support::mip_autogen;
inline constexpr Support integer_texel =
    textures | Support::buffer | Support::shader_load | Support::shader_gather |
    Support::render_target | Support::multisample_render_target | Support::typed_uav_store;
inline constexpr Support integer_color = integer_texel | Support::vertex_buffer;
inline constexpr Support typeless_color = textures | Support::buffer;
// Metal's RGB pixel formats exist only as texture buffers (Apple11 and later); vertex fetch uses
// MTLVertexFormat instead.
inline constexpr Support rgb_buffer =
    Support::buffer | Support::vertex_buffer | Support::shader_load;
inline constexpr Support packed_16bit_color =
    textures | sampled | renderable | Support::mip_autogen;
// Metal has no 1D or 3D depth and stencil textures.
inline constexpr Support depth_textures = Support::texture2d | Support::texture_cube;
inline constexpr Support depth_stencil_target =
    depth_textures | Support::depth_stencil | Support::multisample_render_target;
inline constexpr Support depth_read = depth_textures | sampled;
inline constexpr Support stencil_read = depth_textures | Support::shader_load;
inline constexpr Support compressed_typeless =
    Support::texture2d | Support::texture3d | Support::texture_cube;
inline constexpr Support compressed_color = compressed_typeless | sampled;
// Rows add Support::typed_uav_load only for R32 formats and the TypedUAVLoadAdditionalFormats
// set; Metal's tier 2 read-write textures cover the same set.

} // namespace directmetal::formats::tables
