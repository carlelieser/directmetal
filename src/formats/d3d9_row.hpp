#pragma once

#include "dxgi_row.hpp"

#include <directmetal/formats/formats.hpp>

#include <array>
#include <cstdint>
#include <string_view>

// Shared vocabulary of the D3DFORMAT tables. Direct3D 9 names packed formats from the most
// significant bits, so D3DFMT_A8R8G8B8 is DXGI_FORMAT_B8G8R8A8_UNORM. Direct3D 9 returns one for
// channels a format lacks (https://learn.microsoft.com/windows/win32/direct3d9/d3dformat).
namespace directmetal::formats::tables {

constexpr D3d9Usage operator|(D3d9Usage left, D3d9Usage right) noexcept {
    return static_cast<D3d9Usage>(static_cast<std::uint32_t>(left) |
                                  static_cast<std::uint32_t>(right));
}

// MAKEFOURCC from d3d9types.h: the first character in the low byte.
constexpr std::uint32_t fourcc(std::string_view code) noexcept {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(code[0])) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(code[1])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(code[2])) << 16 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(code[3])) << 24;
}

inline constexpr std::array<Swizzle, 4> luminance_swizzle = {Swizzle::red, Swizzle::red,
                                                             Swizzle::red, Swizzle::one};
inline constexpr std::array<Swizzle, 4> luminance_alpha_swizzle = {Swizzle::red, Swizzle::red,
                                                                   Swizzle::red, Swizzle::green};
inline constexpr std::array<Swizzle, 4> red_swizzle = {Swizzle::red, Swizzle::one, Swizzle::one,
                                                       Swizzle::one};
inline constexpr std::array<Swizzle, 4> red_green_swizzle = {Swizzle::red, Swizzle::green,
                                                             Swizzle::one, Swizzle::one};
inline constexpr std::array<Swizzle, 4> x4r4g4b4_swizzle = {Swizzle::green, Swizzle::blue,
                                                            Swizzle::alpha, Swizzle::one};

inline constexpr D3d9Usage d3d9_textures =
    D3d9Usage::texture | D3d9Usage::cube_texture | D3d9Usage::volume_texture;
inline constexpr D3d9Usage d3d9_sampled = d3d9_textures | D3d9Usage::vertex_texture;
inline constexpr D3d9Usage d3d9_filtered = d3d9_sampled | D3d9Usage::filter;
inline constexpr D3d9Usage d3d9_target =
    d3d9_filtered | D3d9Usage::render_target | D3d9Usage::blend | D3d9Usage::auto_gen_mipmap;
inline constexpr D3d9Usage d3d9_srgb = D3d9Usage::srgb_read | D3d9Usage::srgb_write;
// Apple7 and Apple8 do not filter 32-bit float formats.
inline constexpr D3d9Usage d3d9_float32_target =
    d3d9_sampled | D3d9Usage::render_target | D3d9Usage::blend;
inline constexpr D3d9Usage d3d9_compressed =
    d3d9_textures | D3d9Usage::filter | D3d9Usage::srgb_read;
inline constexpr D3d9Usage d3d9_packed_422 = D3d9Usage::texture | D3d9Usage::filter;
inline constexpr D3d9Usage d3d9_sampled_depth = D3d9Usage::texture | D3d9Usage::depth_stencil;

inline constexpr Substitution no_substitution = Substitution::none;
inline constexpr Substitution forced_alpha = Substitution::alpha_forced_one;

} // namespace directmetal::formats::tables
