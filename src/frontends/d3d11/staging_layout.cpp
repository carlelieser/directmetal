#include "staging_layout.hpp"

#include <directmetal/backend/types.hpp>

#include <algorithm>

namespace directmetal::d3d11 {
namespace {

constexpr std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

std::uint64_t blocks(std::uint32_t size, std::uint32_t block) noexcept {
    return (std::uint64_t{size} + block - 1) / block;
}

struct Pitches {
    std::uint64_t row;
    std::uint64_t depth;
};

Pitches pitches_of(const formats::FormatInfo& format, std::uint32_t width,
                   std::uint32_t height) noexcept {
    const std::uint64_t row_bytes = blocks(width, format.block_width) * format.bytes_per_block;
    const std::uint64_t row = align_up(row_bytes, StagingLayout::staging_row_alignment);
    return {row, row * blocks(height, format.block_height)};
}

// Calls visit(mip, width, height, pitches) for every subresource in Direct3D order and returns
// the bytes they span.
template <typename Visit>
std::uint64_t walk(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo& format,
                   Visit&& visit) {
    std::uint64_t offset = 0;
    for (UINT slice = 0; slice < desc.ArraySize; ++slice) {
        for (UINT mip = 0; mip < desc.MipLevels; ++mip) {
            const std::uint32_t width = mip_size(desc.Width, mip);
            const std::uint32_t height = mip_size(desc.Height, mip);
            const Pitches pitches = pitches_of(format, width, height);
            visit(offset, width, height, pitches);
            offset += align_up(pitches.depth, StagingLayout::staging_row_alignment);
        }
    }
    return offset;
}

} // namespace

std::uint32_t mip_size(std::uint32_t size, std::uint32_t mip) noexcept {
    return std::max(size >> mip, 1u);
}

std::uint64_t StagingLayout::required_bytes(const D3D11_TEXTURE2D_DESC& desc,
                                            const formats::FormatInfo& format) noexcept {
    return walk(desc, format, [](std::uint64_t, std::uint32_t, std::uint32_t, Pitches) {});
}

StagingLayout::StagingLayout(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo& format) {
    subresources_.reserve(std::size_t{desc.MipLevels} * desc.ArraySize);
    const std::uint64_t bytes = walk(
        desc, format,
        [this](std::uint64_t offset, std::uint32_t width, std::uint32_t height, Pitches pitches) {
            subresources_.push_back({offset, static_cast<std::uint32_t>(pitches.row),
                                     static_cast<std::uint32_t>(pitches.depth), width, height});
        });
    size_ = align_up(std::max<std::uint64_t>(bytes, 1), backend::import_alignment);
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
