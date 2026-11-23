#pragma once

#include "pixel_layout.hpp"

#include <directmetal/backend/types.hpp>

#include <cstdint>

// Rules for the descriptions behind create_buffer and create_texture, and the texture geometry
// the stream's commands are checked against.
namespace directmetal::host {

bool is_valid_buffer_desc(const backend::BufferDesc& desc,
                          const backend::Capabilities& capabilities) noexcept;
bool is_valid_texture_desc(const backend::TextureDesc& desc,
                           const backend::Capabilities& capabilities) noexcept;

// Array slices, counting each cube face; 1 for non-array types, including 3D.
std::uint32_t slice_count(const backend::TextureDesc& desc) noexcept;
bool is_multisample(const backend::TextureDesc& desc) noexcept;
// The extent of mip, which must be below desc.mip_levels.
backend::Extent3D mip_extent(const backend::TextureDesc& desc, std::uint32_t mip) noexcept;

} // namespace directmetal::host
