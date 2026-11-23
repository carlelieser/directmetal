#include <directmetal/formats/formats.hpp>

namespace directmetal::formats {
namespace {

// Views name a typed format; video resources are not viewable in Phase 1.
bool is_view_format(const FormatInfo& view) noexcept {
    return view.kind != ComponentKind::typeless && view.kind != ComponentKind::video;
}

bool is_in_family(const FormatInfo& resource, const FormatInfo& view) noexcept {
    const bool has_family = view.typeless_family != 0;
    return has_family && view.typeless_family == resource.typeless_family;
}

bool is_depth(const FormatInfo& info) noexcept {
    return info.kind == ComponentKind::depth || info.kind == ComponentKind::depth_stencil;
}

// Relaxed casting reinterprets bits of one color plane; it does not reach depth formats or the
// plane views of depth-stencil families.
bool is_relaxed_castable(const FormatInfo& resource, const FormatInfo& view) noexcept {
    const bool is_single_plane = resource.plane_count == 1 && view.plane_count == 1;
    const bool has_depth = is_depth(resource) || is_depth(view);
    return is_single_plane && !has_depth && resource.bytes_per_block == view.bytes_per_block;
}

} // namespace

bool is_view_compatible(DxgiFormat resource, DxgiFormat view, CastingRules rules) noexcept {
    const FormatInfo* resource_info = find(resource);
    const FormatInfo* view_info = find(view);
    if (resource_info == nullptr || view_info == nullptr || !is_view_format(*view_info)) {
        return false;
    }
    if (resource == view) {
        return true;
    }
    if (!is_in_family(*resource_info, *view_info)) {
        return false;
    }
    if (resource_info->kind == ComponentKind::typeless) {
        return true;
    }
    return rules == CastingRules::d3d12_relaxed && is_relaxed_castable(*resource_info, *view_info);
}

} // namespace directmetal::formats
