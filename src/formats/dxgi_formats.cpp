#include "dxgi_table.hpp"
#include "metal_capabilities.hpp"

#include <directmetal/formats/formats.hpp>

#include <algorithm>

namespace directmetal::formats {
namespace {

// A usage Direct3D allows is reported when the row's pixel format has every listed capability.
// Vertex, index and display usages need no texture capability.
struct Requirement {
    Support usage;
    metal::Capability capabilities;
};

constexpr Support needs_no_capability =
    Support::vertex_buffer | Support::index_buffer | Support::display;

constexpr Requirement requirements[] = {
    {tables::textures, metal::Capability::texture},
    {Support::buffer, metal::Capability::texture_buffer},
    {Support::shader_load, metal::Capability::texture},
    {Support::shader_sample, metal::Capability::filter},
    // Gather reads four texels without filtering them; comparison samplers filter the results of
    // comparisons, not texels.
    {Support::shader_gather, metal::Capability::texture},
    {Support::sample_comparison, metal::Capability::texture},
    {Support::gather_comparison, metal::Capability::texture},
    {Support::render_target, metal::Capability::render},
    {Support::blendable, metal::Capability::blend},
    {Support::depth_stencil, metal::Capability::render},
    {Support::multisample_render_target, metal::Capability::msaa},
    {Support::multisample_resolve, metal::Capability::resolve},
    {Support::typed_uav_load, metal::Capability::read_write},
    {Support::typed_uav_store, metal::Capability::write},
    // MTLBlitCommandEncoder generateMipmaps needs a color-renderable, filterable format.
    {Support::mip_autogen, metal::Capability::render | metal::Capability::filter},
};

const tables::DxgiRow* find_row(DxgiFormat format) noexcept {
    const auto row = std::lower_bound(tables::dxgi_rows.begin(), tables::dxgi_rows.end(), format,
                                      [](const tables::DxgiRow& candidate, DxgiFormat value) {
                                          return candidate.format < value;
                                      });
    const bool is_found = row != tables::dxgi_rows.end() && row->format == format;
    return is_found ? &*row : nullptr;
}

bool has_feature(const metal::CapabilityRow& row, const DeviceFeatures& features) noexcept {
    return row.feature == nullptr || features.*row.feature;
}

metal::Capability capabilities_of(backend::PixelFormat pixel_format,
                                  const DeviceFeatures& features) noexcept {
    metal::Capability capabilities = metal::Capability::none;
    for (const metal::CapabilityRow& row : metal::capability_rows) {
        const bool is_available = row.pixel_format == pixel_format && has_feature(row, features);
        if (is_available) {
            capabilities = capabilities | row.capabilities;
        }
    }
    return capabilities;
}

} // namespace

const FormatInfo* find(DxgiFormat format) noexcept {
    const tables::DxgiRow* row = find_row(format);
    if (row == nullptr) {
        return nullptr;
    }
    return &tables::dxgi_format_infos[static_cast<std::size_t>(row - tables::dxgi_rows.data())];
}

std::span<const FormatInfo> all_dxgi_formats() noexcept {
    return tables::dxgi_format_infos;
}

DeviceFeatures mac_features(backend::GpuFamily family) noexcept {
    const bool is_apple9_or_later = family >= backend::GpuFamily::apple9;
    return {family, is_apple9_or_later, true, true};
}

Support support(DxgiFormat format, backend::GpuFamily family) noexcept {
    return support(format, mac_features(family));
}

Support support(DxgiFormat format, const DeviceFeatures& features) noexcept {
    const tables::DxgiRow* row = find_row(format);
    if (row == nullptr) {
        return Support::none;
    }
    const metal::Capability capabilities = capabilities_of(row->pixel_format, features);
    Support result = row->usage & needs_no_capability;
    for (const Requirement& requirement : requirements) {
        if (metal::has_all(capabilities, requirement.capabilities)) {
            result = result | (row->usage & requirement.usage);
        }
    }
    return result;
}

} // namespace directmetal::formats
