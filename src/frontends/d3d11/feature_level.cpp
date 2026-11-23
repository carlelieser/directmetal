#include "feature_level.hpp"

#include <algorithm>
#include <array>

namespace directmetal::d3d11 {
namespace {

// The levels D3D11CreateDevice tries when the application passes none
// (https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice).
constexpr std::array default_levels = {
    D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1,
};

constexpr std::array known_levels = {
    D3D_FEATURE_LEVEL_9_1,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1,
};

constexpr std::array supported_levels = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1};

template <typename Levels> bool contains(const Levels& levels, D3D_FEATURE_LEVEL level) noexcept {
    return std::find(levels.begin(), levels.end(), level) != levels.end();
}

} // namespace

HRESULT negotiate_feature_level(std::span<const D3D_FEATURE_LEVEL> requested,
                                D3D_FEATURE_LEVEL& chosen) noexcept {
    const std::span<const D3D_FEATURE_LEVEL> candidates =
        requested.empty() ? std::span<const D3D_FEATURE_LEVEL>(default_levels) : requested;
    const bool is_all_known =
        std::all_of(candidates.begin(), candidates.end(),
                    [](D3D_FEATURE_LEVEL level) { return contains(known_levels, level); });
    if (!is_all_known) {
        return E_INVALIDARG;
    }
    const auto found =
        std::find_if(candidates.begin(), candidates.end(),
                     [](D3D_FEATURE_LEVEL level) { return contains(supported_levels, level); });
    if (found == candidates.end()) {
        return DXGI_ERROR_UNSUPPORTED;
    }
    chosen = *found;
    return S_OK;
}

} // namespace directmetal::d3d11
