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

// One subresource, before its pitches are narrowed to SubresourceLayout's 32 bits.
struct Placement {
    std::uint64_t offset;
    std::uint64_t row_pitch;
    std::uint64_t depth_pitch;
    std::uint32_t width;
    std::uint32_t height;
};

Placement placement_of(const formats::FormatInfo& format, std::uint32_t width,
                       std::uint32_t height) noexcept {
    const std::uint64_t row_bytes = blocks(width, format.block_width) * format.bytes_per_block;
    const std::uint64_t row = align_up(row_bytes, StagingLayout::staging_row_alignment);
    return {0, row, row * blocks(height, format.block_height), width, height};
}

// Calls visit with every subresource in Direct3D order and returns the bytes they span.
template <typename Visit>
std::uint64_t walk(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo& format,
                   Visit&& visit) {
    std::uint64_t offset = 0;
    for (UINT slice = 0; slice < desc.ArraySize; ++slice) {
        for (UINT mip = 0; mip < desc.MipLevels; ++mip) {
            Placement placement =
                placement_of(format, mip_size(desc.Width, mip), mip_size(desc.Height, mip));
            placement.offset = offset;
            visit(placement);
            offset += align_up(placement.depth_pitch, StagingLayout::staging_row_alignment);
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
    return walk(desc, format, [](const Placement&) {});
}

StagingLayout::StagingLayout(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo& format) {
    subresources_.reserve(std::size_t{desc.MipLevels} * desc.ArraySize);
    const std::uint64_t bytes = walk(desc, format, [this](const Placement& placement) {
        subresources_.push_back({placement.offset, static_cast<std::uint32_t>(placement.row_pitch),
                                 static_cast<std::uint32_t>(placement.depth_pitch), placement.width,
                                 placement.height});
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
