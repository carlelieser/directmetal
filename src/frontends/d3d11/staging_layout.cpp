#include "staging_layout.hpp"

#include <directmetal/backend/types.hpp>

#include <algorithm>

namespace directmetal::d3d11 {
namespace {

constexpr std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

std::uint32_t blocks(std::uint32_t size, std::uint32_t block) noexcept {
    return (size + block - 1) / block;
}

SubresourceLayout layout_of(const formats::FormatInfo& format, std::uint32_t width,
                            std::uint32_t height) noexcept {
    const std::uint64_t row_bytes =
        std::uint64_t{blocks(width, format.block_width)} * format.bytes_per_block;
    const auto row_pitch =
        static_cast<std::uint32_t>(align_up(row_bytes, StagingLayout::staging_row_alignment));
    const std::uint32_t rows = blocks(height, format.block_height);
    return {0, row_pitch, row_pitch * rows, width, height};
}

} // namespace

std::uint32_t mip_size(std::uint32_t size, std::uint32_t mip) noexcept {
    return std::max(size >> mip, 1u);
}

StagingLayout::StagingLayout(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo& format) {
    subresources_.reserve(std::size_t{desc.MipLevels} * desc.ArraySize);
    std::uint64_t offset = 0;
    for (UINT slice = 0; slice < desc.ArraySize; ++slice) {
        for (UINT mip = 0; mip < desc.MipLevels; ++mip) {
            SubresourceLayout layout =
                layout_of(format, mip_size(desc.Width, mip), mip_size(desc.Height, mip));
            layout.offset = offset;
            offset += align_up(layout.depth_pitch, staging_row_alignment);
            subresources_.push_back(layout);
        }
    }
    size_ = align_up(std::max<std::uint64_t>(offset, 1), backend::import_alignment);
}

std::uint64_t StagingLayout::size() const noexcept {
    return size_;
}

std::uint32_t StagingLayout::subresource_count() const noexcept {
    return static_cast<std::uint32_t>(subresources_.size());
}

const SubresourceLayout& StagingLayout::at(std::uint32_t subresource) const noexcept {
    return subresources_[subresource];
}

} // namespace directmetal::d3d11
