#pragma once

#include <directmetal/formats/formats.hpp>

#include <d3d11.h>

#include <cstdint>
#include <vector>

namespace directmetal::d3d11 {

// Where one subresource of a staging texture lies in its memory.
struct SubresourceLayout {
    std::uint64_t offset;
    std::uint32_t row_pitch;
    std::uint32_t depth_pitch;
    std::uint32_t width;
    std::uint32_t height;
};

// Subresources in Direct3D order (D3D11CalcSubresource: mip + slice * mip levels), rows aligned to
// staging_row_alignment. desc.MipLevels must already be resolved.
class StagingLayout {
public:
    static constexpr std::uint32_t staging_row_alignment = 256;

    StagingLayout(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo& format);

    // Total bytes, rounded up to the shared memory alignment so the memory can be imported.
    std::uint64_t size() const noexcept;
    std::uint32_t subresource_count() const noexcept;
    const SubresourceLayout& at(std::uint32_t subresource) const noexcept;

private:
    std::vector<SubresourceLayout> subresources_;
    std::uint64_t size_ = 0;
};

// The extent of `mip` for a texture of `size` texels on that axis.
std::uint32_t mip_size(std::uint32_t size, std::uint32_t mip) noexcept;

} // namespace directmetal::d3d11
