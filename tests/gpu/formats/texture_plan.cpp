#include "texture_plan.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

namespace directmetal::formats::gpu_testing {
namespace {

using backend::PixelFormat;
using backend::TextureType;
using backend::TextureUsage;

struct Dimension {
    Support claim;
    TextureType type;
    backend::Extent3D extent;
    std::string_view label;
    // Metal cannot render to 1D textures.
    bool can_render;
};

constexpr Dimension dimensions[] = {
    {Support::texture1d, TextureType::tex1d, {8, 1, 1}, "1d", false},
    {Support::texture2d, TextureType::tex2d, {8, 8, 1}, "2d", true},
    {Support::texture3d, TextureType::tex3d, {8, 8, 2}, "3d", true},
    {Support::texture_cube, TextureType::cube, {8, 8, 1}, "cube", true},
};

constexpr Support depth_dimensions = Support::texture2d | Support::texture_cube;

struct UsageRule {
    Support claims;
    TextureUsage usage;
};

constexpr UsageRule usage_rules[] = {
    {Support::shader_load | Support::shader_sample | Support::shader_gather |
         Support::sample_comparison | Support::gather_comparison,
     TextureUsage::shader_read},
    {Support::typed_uav_load | Support::typed_uav_store, TextureUsage::shader_write},
    {Support::render_target | Support::depth_stencil | Support::multisample_render_target,
     TextureUsage::render_target},
};

struct CopyBytes {
    PixelFormat pixel_format;
    std::uint32_t block_bytes;
};

// Buffer copies of combined formats move the depth aspect only.
constexpr CopyBytes depth_copy_bytes[] = {
    {PixelFormat::depth16_unorm, 2},          {PixelFormat::depth32_float, 4},
    {PixelFormat::depth32_float_stencil8, 4}, {PixelFormat::stencil8, 1},
    {PixelFormat::x32_stencil8, 1},
};

constexpr std::uint8_t depth_pattern_byte = 0x3c;

TextureUsage without(TextureUsage usage, TextureUsage removed) {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(usage) &
                                     ~static_cast<std::uint32_t>(removed));
}

backend::TextureDesc base_desc(const Dimension& dimension, PixelFormat pixel_format,
                               TextureUsage usage) {
    return {dimension.type,
            backend::StorageMode::gpu_private,
            0,
            pixel_format,
            dimension.extent,
            1,
            1,
            1,
            usage};
}

Check check_for(Support claimed, const Dimension& dimension) {
    if (!dimension.can_render) {
        return Check::round_trip;
    }
    if (has_any(claimed, Support::depth_stencil)) {
        return Check::depth_clear;
    }
    return has_any(claimed, Support::render_target) ? Check::color_clear : Check::round_trip;
}

PlannedTexture color_plan(const FormatInfo& info, Support claimed, const Dimension& dimension) {
    TextureUsage usage = claimed_usage(info, claimed);
    if (!dimension.can_render) {
        usage = without(usage, TextureUsage::render_target);
    }
    const backend::TextureDesc desc = base_desc(dimension, resource_pixel_format(info), usage);
    return {dimension.label, desc, check_for(claimed, dimension), copy_layout(info, desc)};
}

// A typeless family bound as a depth-stencil target, also read through its plane views.
PlannedTexture depth_plan(const FormatInfo& info, const Dimension& dimension) {
    const TextureUsage usage =
        TextureUsage::shader_read | TextureUsage::render_target | TextureUsage::pixel_format_view;
    const backend::TextureDesc desc = base_desc(dimension, info.depth_pixel_format, usage);
    return {dimension.type == TextureType::cube ? "depth_cube" : "depth_2d", desc,
            Check::depth_clear, copy_layout(info, desc)};
}

} // namespace

bool is_view_only(PixelFormat pixel_format) noexcept {
    return pixel_format == PixelFormat::x32_stencil8 || pixel_format == PixelFormat::x24_stencil8;
}

bool is_depth_or_stencil(PixelFormat pixel_format) noexcept {
    return std::ranges::any_of(depth_copy_bytes, [pixel_format](const CopyBytes& entry) {
        return entry.pixel_format == pixel_format;
    });
}

PixelFormat resource_pixel_format(const FormatInfo& info) noexcept {
    if (!is_view_only(info.pixel_format)) {
        return info.pixel_format;
    }
    const FormatInfo* family = find(info.typeless_family);
    return family == nullptr ? PixelFormat::invalid : family->depth_pixel_format;
}

TextureUsage claimed_usage(const FormatInfo& info, Support claimed) noexcept {
    TextureUsage usage = TextureUsage::none;
    for (const UsageRule& rule : usage_rules) {
        if (has_any(claimed, rule.claims)) {
            usage = usage | rule.usage;
        }
    }
    const bool is_viewed =
        info.kind == ComponentKind::typeless || resource_pixel_format(info) != info.pixel_format;
    return is_viewed ? usage | TextureUsage::pixel_format_view : usage;
}

std::uint32_t copy_block_bytes(const FormatInfo& info, PixelFormat pixel_format) noexcept {
    const auto found = std::ranges::find(depth_copy_bytes, pixel_format, &CopyBytes::pixel_format);
    return found == std::end(depth_copy_bytes) ? info.bytes_per_block : found->block_bytes;
}

CopyLayout copy_layout(const FormatInfo& info, const backend::TextureDesc& desc) {
    const std::uint32_t block_bytes = copy_block_bytes(info, desc.format);
    const bool is_depth_format = is_depth_or_stencil(desc.format);
    const std::uint32_t block_width = is_depth_format ? 1 : info.block_width;
    const std::uint32_t block_height = is_depth_format ? 1 : info.block_height;
    const std::uint32_t columns = (desc.extent.width + block_width - 1) / block_width;
    const std::uint32_t rows = (desc.extent.height + block_height - 1) / block_height;
    const std::uint32_t slices = desc.type == TextureType::cube ? 6 : desc.array_length;
    return {desc.extent, columns * block_bytes, columns * block_bytes * rows, slices, block_bytes};
}

std::vector<PlannedTexture> plan_textures(const FormatInfo& info, Support claimed) {
    std::vector<PlannedTexture> planned;
    for (const Dimension& dimension : dimensions) {
        if (has_any(claimed, dimension.claim)) {
            planned.push_back(color_plan(info, claimed, dimension));
        }
    }
    const bool is_depth_family = info.depth_pixel_format != PixelFormat::invalid &&
                                 !has_any(claimed, Support::depth_stencil);
    for (const Dimension& dimension : dimensions) {
        if (is_depth_family && has_any(claimed & depth_dimensions, dimension.claim)) {
            planned.push_back(depth_plan(info, dimension));
        }
    }
    return planned;
}

std::vector<std::byte> upload_pattern(const PlannedTexture& planned) {
    std::vector<std::byte> bytes(planned.layout.total_bytes());
    const bool is_depth_format = is_depth_or_stencil(planned.desc.format);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto varying = static_cast<std::uint8_t>(index * 37 + 11);
        bytes[index] = std::byte{is_depth_format ? depth_pattern_byte : varying};
    }
    return bytes;
}

std::vector<std::byte> expected_readback(const PlannedTexture& planned) {
    if (planned.check == Check::round_trip) {
        return upload_pattern(planned);
    }
    std::vector<std::byte> bytes(planned.layout.total_bytes());
    if (planned.check == Check::color_clear) {
        return bytes;
    }
    const auto one = std::bit_cast<std::array<std::byte, 4>>(1.0f);
    const bool is_unorm16 = planned.desc.format == PixelFormat::depth16_unorm;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = is_unorm16 ? std::byte{0xff} : one[index % one.size()];
    }
    return bytes;
}

std::string short_name(const FormatInfo& info) {
    constexpr std::string_view prefix = "DXGI_FORMAT_";
    const std::string_view name = info.name;
    return std::string(name.starts_with(prefix) ? name.substr(prefix.size()) : name);
}

} // namespace directmetal::formats::gpu_testing
