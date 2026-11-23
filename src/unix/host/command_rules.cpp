#include "command_rules.hpp"

#include "pixel_layout.hpp"
#include "texture_rules.hpp"

namespace directmetal::host {
namespace {

using backend::TextureDesc;

bool is_span_within(std::uint64_t first, std::uint64_t count, std::uint64_t limit) noexcept {
    return count >= 1 && first < limit && count <= limit - first;
}

bool is_valid_range(const TextureDesc& desc, const backend::SubresourceRange& range) noexcept {
    const bool has_mips = is_span_within(range.first_mip, range.mip_count, desc.mip_levels);
    return has_mips && is_span_within(range.first_slice, range.slice_count, slice_count(desc));
}

bool is_render_target(const TextureDesc& desc) noexcept {
    return (desc.usage & backend::TextureUsage::render_target) != backend::TextureUsage::none;
}

// One axis of a region: inside the mip and, for block-compressed formats, on block boundaries
// except where it reaches the mip's edge.
struct Axis {
    std::uint32_t origin;
    std::uint32_t size;
    std::uint32_t limit;
};

bool is_axis_valid(Axis axis, std::uint32_t block) noexcept {
    if (!is_span_within(axis.origin, axis.size, axis.limit)) {
        return false;
    }
    const bool reaches_edge = axis.origin + axis.size == axis.limit;
    return axis.origin % block == 0 && (axis.size % block == 0 || reaches_edge);
}

bool is_valid_region(const TextureDesc& desc, const TextureRegion& region) noexcept {
    const PixelLayout* layout = find_layout(desc.format);
    const bool is_subresource = region.mip < desc.mip_levels && region.slice < slice_count(desc);
    if (layout == nullptr || !is_subresource) {
        return false;
    }
    const backend::Extent3D limit = mip_extent(desc, region.mip);
    const bool is_x =
        is_axis_valid({region.origin.x, region.extent.width, limit.width}, layout->block_width);
    const bool is_y =
        is_axis_valid({region.origin.y, region.extent.height, limit.height}, layout->block_height);
    return is_x && is_y && is_axis_valid({region.origin.z, region.extent.depth, limit.depth}, 1);
}

bool are_disjoint(std::uint64_t first, std::uint64_t second, std::uint64_t size) noexcept {
    return first + size <= second || second + size <= first;
}

std::uint64_t blocks(std::uint32_t size, std::uint32_t block) noexcept {
    return (static_cast<std::uint64_t>(size) + block - 1) / block;
}

// Bytes the copy touches from region.offset, for a region already inside its texture.
bool fits_buffer(const bridge::BufferTextureRegion& region, std::uint64_t buffer_size,
                 const PixelLayout& layout) noexcept {
    const std::uint64_t row_bytes =
        blocks(region.extent.width, layout.block_width) * layout.bytes_per_block;
    const std::uint64_t rows = blocks(region.extent.height, layout.block_height);
    const bool is_pitch_aligned = region.row_pitch % layout.bytes_per_block == 0;
    if (region.row_pitch < row_bytes || !is_pitch_aligned) {
        return false;
    }
    const std::uint64_t image_bytes = (rows - 1) * region.row_pitch + row_bytes;
    const bool is_flat = region.extent.depth == 1;
    if (!is_flat && region.slice_pitch < rows * region.row_pitch) {
        return false;
    }
    const std::uint64_t total =
        static_cast<std::uint64_t>(region.extent.depth - 1) * region.slice_pitch + image_bytes;
    const bool is_offset_aligned = region.offset % layout.bytes_per_block == 0;
    return is_offset_aligned && region.offset <= buffer_size &&
           total <= buffer_size - region.offset;
}

std::uint8_t clear_aspects_of(const PixelLayout& layout) noexcept {
    std::uint8_t aspects = 0;
    if (has_aspect(layout, Aspect::depth)) {
        aspects |= static_cast<std::uint8_t>(backend::ClearAspect::depth);
    }
    if (has_aspect(layout, Aspect::stencil)) {
        aspects |= static_cast<std::uint8_t>(backend::ClearAspect::stencil);
    }
    return aspects;
}

// Buffer copies of combined depth-stencil formats would need a per-aspect copy option.
bool is_single_aspect(const PixelLayout& layout) noexcept {
    return !(has_aspect(layout, Aspect::depth) && has_aspect(layout, Aspect::stencil));
}

// Metal on macOS copies between buffers only at offsets and sizes that are multiples of 4.
constexpr std::uint64_t buffer_copy_alignment = 4;

bool is_copy_aligned(const bridge::CopyBufferCommand& command) noexcept {
    const std::uint64_t bits = command.source_offset | command.destination_offset | command.size;
    return bits % buffer_copy_alignment == 0;
}

} // namespace

bool is_valid_color_clear(const TextureDesc& desc,
                          const bridge::ClearTextureCommand& command) noexcept {
    const PixelLayout* layout = find_layout(desc.format);
    const bool is_color = layout != nullptr && has_aspect(*layout, Aspect::color);
    const bool is_known_kind = command.color.kind <= backend::ClearKind::sint_value;
    if (!is_color || !is_known_kind) {
        return false;
    }
    return is_render_target(desc) && is_valid_range(desc, command.range);
}

bool is_valid_depth_clear(const TextureDesc& desc,
                          const bridge::ClearDepthStencilCommand& command) noexcept {
    const PixelLayout* layout = find_layout(desc.format);
    if (layout == nullptr || !is_render_target(desc)) {
        return false;
    }
    const auto requested = static_cast<std::uint8_t>(command.clear.aspects);
    const std::uint8_t available = clear_aspects_of(*layout);
    const bool is_aspect_subset = requested != 0 && (requested & ~available) == 0;
    const bool is_depth_in_range = command.clear.depth >= 0.0f && command.clear.depth <= 1.0f;
    return is_aspect_subset && is_depth_in_range && is_valid_range(desc, command.range);
}

bool is_valid_buffer_copy(const bridge::CopyBufferCommand& command,
                          const BufferCopyCheck& check) noexcept {
    if (!is_copy_aligned(command)) {
        return false;
    }
    const bool is_source_valid =
        is_span_within(command.source_offset, command.size, check.source_size);
    const bool is_destination_valid =
        is_span_within(command.destination_offset, command.size, check.destination_size);
    if (!is_source_valid || !is_destination_valid) {
        return false;
    }
    return !check.is_same_buffer ||
           are_disjoint(command.source_offset, command.destination_offset, command.size);
}

bool is_valid_buffer_texture_copy(const bridge::BufferTextureRegion& region,
                                  std::uint64_t buffer_size, const TextureDesc& desc) noexcept {
    const PixelLayout* layout = find_layout(desc.format);
    const bool is_copyable =
        desc.storage != backend::StorageMode::memoryless && !is_multisample(desc);
    if (layout == nullptr || !is_copyable) {
        return false;
    }
    const TextureRegion texture_region{region.mip, region.slice, region.origin, region.extent};
    if (!is_single_aspect(*layout) || !is_valid_region(desc, texture_region)) {
        return false;
    }
    return fits_buffer(region, buffer_size, *layout);
}

bool is_valid_texture_copy(const bridge::CopyTextureCommand& command, const TextureDesc& source,
                           const TextureDesc& destination) noexcept {
    const bool is_compatible =
        source.format == destination.format && source.sample_count == destination.sample_count;
    const bool is_copyable = source.storage != backend::StorageMode::memoryless &&
                             destination.storage != backend::StorageMode::memoryless;
    if (!is_compatible || !is_copyable) {
        return false;
    }
    const TextureRegion from{command.source_mip, command.source_slice, command.source_origin,
                             command.extent};
    const TextureRegion to{command.destination_mip, command.destination_slice,
                           command.destination_origin, command.extent};
    return is_valid_region(source, from) && is_valid_region(destination, to);
}

bool overlaps(const TextureRegion& first, const TextureRegion& second) noexcept {
    if (first.mip != second.mip || first.slice != second.slice) {
        return false;
    }
    const bool is_x_apart = are_disjoint(first.origin.x, second.origin.x, first.extent.width);
    const bool is_y_apart = are_disjoint(first.origin.y, second.origin.y, first.extent.height);
    const bool is_z_apart = are_disjoint(first.origin.z, second.origin.z, first.extent.depth);
    return !(is_x_apart || is_y_apart || is_z_apart);
}

} // namespace directmetal::host
