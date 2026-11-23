#include "texture_rules.hpp"

#include <directmetal/formats/formats.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>

namespace directmetal::host {
namespace {

using backend::TextureDesc;
using backend::TextureType;

struct TypeRule {
    std::uint8_t dimensions;
    bool is_array;
    bool is_cube;
    bool is_multisample;
};

// Indexed by TextureType.
constexpr std::array<TypeRule, 9> type_rules = {{
    {1, false, false, false}, // tex1d
    {1, true, false, false},  // tex1d_array
    {2, false, false, false}, // tex2d
    {2, true, false, false},  // tex2d_array
    {2, false, false, true},  // tex2d_ms
    {2, true, false, true},   // tex2d_ms_array
    {3, false, false, false}, // tex3d
    {2, false, true, false},  // cube
    {2, true, true, false},   // cube_array
}};

// Metal's limit on arrayLength.
constexpr std::uint32_t max_array_length = 2048;

constexpr auto known_usage =
    backend::TextureUsage::shader_read | backend::TextureUsage::shader_write |
    backend::TextureUsage::render_target | backend::TextureUsage::pixel_format_view;

const TypeRule& rule_of(const TextureDesc& desc) noexcept {
    return type_rules[static_cast<std::size_t>(desc.type)];
}

bool has_enumerated_fields(const TextureDesc& desc) noexcept {
    const bool is_known_type = static_cast<std::size_t>(desc.type) < type_rules.size();
    // Memoryless textures only live inside one render pass, which Phase 1 has no command for.
    const bool is_known_storage = desc.storage <= backend::StorageMode::gpu_private;
    const bool is_known_usage = (desc.usage & known_usage) == desc.usage;
    const bool is_known_enum = is_known_type && is_known_storage;
    return is_known_enum && is_known_usage && desc.reserved == 0;
}

std::uint32_t full_mip_chain(const backend::Extent3D& extent) noexcept {
    const std::uint32_t largest = std::max({extent.width, extent.height, extent.depth});
    return static_cast<std::uint32_t>(std::bit_width(largest));
}

bool has_valid_extent(const TextureDesc& desc, const TypeRule& rule) noexcept {
    const backend::Extent3D& extent = desc.extent;
    if (extent.width == 0 || extent.height == 0 || extent.depth == 0) {
        return false;
    }
    const bool is_flat = rule.dimensions == 3 || extent.depth == 1;
    const bool is_line = rule.dimensions != 1 || extent.height == 1;
    const bool is_square = !rule.is_cube || extent.width == extent.height;
    return is_flat && is_line && is_square;
}

bool is_within_limits(const TextureDesc& desc, const TypeRule& rule,
                      const backend::Capabilities& capabilities) noexcept {
    const std::uint32_t limit =
        rule.dimensions == 3 ? capabilities.max_texture_3d : capabilities.max_texture_2d;
    const backend::Extent3D& extent = desc.extent;
    return std::max({extent.width, extent.height, extent.depth}) <= limit;
}

bool has_valid_counts(const TextureDesc& desc, const TypeRule& rule) noexcept {
    const bool is_array_length = desc.array_length >= 1 && desc.array_length <= max_array_length;
    const bool is_valid_array = rule.is_array ? is_array_length : desc.array_length == 1;
    const bool is_valid_mips =
        desc.mip_levels >= 1 && desc.mip_levels <= full_mip_chain(desc.extent);
    if (!is_valid_array || !is_valid_mips) {
        return false;
    }
    if (!rule.is_multisample) {
        return desc.sample_count == 1;
    }
    // Apple GPUs support 2 and 4 samples for every multisample format; 8 only for some.
    const bool is_sample_count = desc.sample_count == 2 || desc.sample_count == 4;
    return is_sample_count && desc.mip_levels == 1;
}

// Depth, stencil and block-compressed formats exist only as 2D, array or cube textures, and
// compressed ones are never render targets.
bool suits_format(const TextureDesc& desc, const TypeRule& rule) noexcept {
    const PixelLayout* layout = find_layout(desc.format);
    if (layout == nullptr) {
        return false;
    }
    const bool is_special = is_compressed(*layout) || !has_aspect(*layout, Aspect::color);
    if (!is_special) {
        return true;
    }
    const bool is_render_target =
        (desc.usage & backend::TextureUsage::render_target) != backend::TextureUsage::none;
    const bool is_blocked_target = is_compressed(*layout) && is_render_target;
    return rule.dimensions == 2 && !is_blocked_target;
}

bool has_usage(const TextureDesc& desc, backend::TextureUsage usage) noexcept {
    return (desc.usage & usage) != backend::TextureUsage::none;
}

formats::DeviceFeatures features_of(const backend::Capabilities& capabilities) noexcept {
    formats::DeviceFeatures features = formats::mac_features(capabilities.family);
    features.has_bc_compression = capabilities.has_bc != 0;
    return features;
}

// What Apple's feature set tables allow for the format on this device: Metal validation aborts
// the process on anything else.
bool is_supported(const TextureDesc& desc, const TypeRule& rule,
                  const backend::Capabilities& capabilities) noexcept {
    const formats::PixelFormatSupport support =
        formats::pixel_format_support(desc.format, features_of(capabilities));
    const bool is_render_allowed =
        support.is_renderable || !has_usage(desc, backend::TextureUsage::render_target);
    const bool is_write_allowed =
        support.is_writable || !has_usage(desc, backend::TextureUsage::shader_write);
    const bool is_multisample_allowed = support.is_multisample || !rule.is_multisample;
    const bool is_usage_allowed = is_render_allowed && is_write_allowed;
    return support.is_texture && is_usage_allowed && is_multisample_allowed;
}

} // namespace

bool is_valid_buffer_desc(const backend::BufferDesc& desc,
                          const backend::Capabilities& capabilities) noexcept {
    const bool is_reserved_zero = std::all_of(std::begin(desc.reserved), std::end(desc.reserved),
                                              [](std::uint8_t value) { return value == 0; });
    const bool is_valid_size = desc.size > 0 && desc.size <= capabilities.max_buffer_length;
    return is_reserved_zero && is_valid_size && desc.storage <= backend::StorageMode::memoryless;
}

bool is_valid_texture_desc(const TextureDesc& desc,
                           const backend::Capabilities& capabilities) noexcept {
    if (!has_enumerated_fields(desc)) {
        return false;
    }
    const TypeRule& rule = rule_of(desc);
    if (!has_valid_extent(desc, rule) || !is_within_limits(desc, rule, capabilities)) {
        return false;
    }
    if (!has_valid_counts(desc, rule) || !suits_format(desc, rule)) {
        return false;
    }
    return is_supported(desc, rule, capabilities);
}

std::uint32_t slice_count(const TextureDesc& desc) noexcept {
    const TypeRule& rule = rule_of(desc);
    return rule.is_cube ? desc.array_length * 6 : desc.array_length;
}

bool is_multisample(const TextureDesc& desc) noexcept {
    return rule_of(desc).is_multisample;
}

backend::Extent3D mip_extent(const TextureDesc& desc, std::uint32_t mip) noexcept {
    const auto shrink = [mip](std::uint32_t size) { return std::max(size >> mip, 1u); };
    return {shrink(desc.extent.width), shrink(desc.extent.height), shrink(desc.extent.depth)};
}

} // namespace directmetal::host
