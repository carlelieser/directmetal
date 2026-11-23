#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/commands.hpp>

#include <cstdint>

// Checks of decoded commands against the descriptions of the resources they name. Handles are
// resolved by the caller.
namespace directmetal::host {

struct TextureRegion {
    std::uint32_t mip;
    std::uint32_t slice;
    backend::Origin3D origin;
    backend::Extent3D extent;
};

bool is_valid_color_clear(const backend::TextureDesc& desc,
                          const bridge::ClearTextureCommand& command) noexcept;
bool is_valid_depth_clear(const backend::TextureDesc& desc,
                          const bridge::ClearDepthStencilCommand& command) noexcept;
// The sizes of the buffers a copy names; is_same_buffer when it names one buffer twice.
struct BufferCopyCheck {
    std::uint64_t source_size;
    std::uint64_t destination_size;
    bool is_same_buffer;
};
bool is_valid_buffer_copy(const bridge::CopyBufferCommand& command,
                          const BufferCopyCheck& check) noexcept;
bool is_valid_buffer_texture_copy(const bridge::BufferTextureRegion& region,
                                  std::uint64_t buffer_size,
                                  const backend::TextureDesc& desc) noexcept;
bool is_valid_texture_copy(const bridge::CopyTextureCommand& command,
                           const backend::TextureDesc& source,
                           const backend::TextureDesc& destination) noexcept;
// Whether two regions of one texture share a texel.
bool overlaps(const TextureRegion& first, const TextureRegion& second) noexcept;

} // namespace directmetal::host
