#include <directmetal/formats/formats.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace directmetal::formats {
namespace {

// Views name a typed format; video resources are not viewable in Phase 1.
bool is_view_format(const FormatInfo& view) noexcept {
    constexpr ComponentKind unviewable[] = {ComponentKind::typeless, ComponentKind::video,
                                            ComponentKind::opaque};
    return std::find(std::begin(unviewable), std::end(unviewable), view.kind) ==
           std::end(unviewable);
}

bool is_in_family(const FormatInfo& resource, const FormatInfo& view) noexcept {
    const bool has_family = view.typeless_family != 0;
    return has_family && view.typeless_family == resource.typeless_family;
}

// Relaxed casting never crosses between float and non-float or between SNORM and UNORM
// (https://microsoft.github.io/DirectX-Specs/d3d/RelaxedCasting.html); sRGB counts as UNORM.
enum class FloatClass : std::uint8_t { none, signed_float, unsigned_float };
enum class Normalization : std::uint8_t { none, unorm, snorm };

struct KindTraits {
    ComponentKind kind;
    FloatClass float_class;
    Normalization normalization;
};

// Depth, typeless, video and opaque kinds are absent: Metal cannot view depth formats as color.
constexpr KindTraits castable_kinds[] = {
    {ComponentKind::unorm, FloatClass::none, Normalization::unorm},
    {ComponentKind::unorm_srgb, FloatClass::none, Normalization::unorm},
    {ComponentKind::snorm, FloatClass::none, Normalization::snorm},
    {ComponentKind::uint, FloatClass::none, Normalization::none},
    {ComponentKind::sint, FloatClass::none, Normalization::none},
    {ComponentKind::float_value, FloatClass::signed_float, Normalization::none},
    {ComponentKind::compressed_unorm, FloatClass::none, Normalization::unorm},
    {ComponentKind::compressed_unorm_srgb, FloatClass::none, Normalization::unorm},
    {ComponentKind::compressed_snorm, FloatClass::none, Normalization::snorm},
    {ComponentKind::compressed_ufloat, FloatClass::unsigned_float, Normalization::none},
    {ComponentKind::compressed_sfloat, FloatClass::signed_float, Normalization::none},
};

const KindTraits* find_traits(ComponentKind kind) noexcept {
    const auto* entry =
        std::find_if(std::begin(castable_kinds), std::end(castable_kinds),
                     [kind](const KindTraits& candidate) { return candidate.kind == kind; });
    return entry == std::end(castable_kinds) ? nullptr : entry;
}

bool is_norm_compatible(Normalization left, Normalization right) noexcept {
    const bool is_either_unnormalized = left == Normalization::none || right == Normalization::none;
    return is_either_unnormalized || left == right;
}

// Relaxed casting reinterprets the bits of one color plane; the plane views of depth-stencil
// families are never cast.
bool is_relaxed_castable(const FormatInfo& resource, const FormatInfo& view) noexcept {
    const KindTraits* resource_traits = find_traits(resource.kind);
    const KindTraits* view_traits = find_traits(view.kind);
    const bool is_single_plane = resource.plane_count == 1 && view.plane_count == 1;
    if (!is_single_plane || resource_traits == nullptr || view_traits == nullptr) {
        return false;
    }
    return resource_traits->float_class == view_traits->float_class &&
           is_norm_compatible(resource_traits->normalization, view_traits->normalization);
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
