#pragma once

#include <d3d11.h>

#include <span>

namespace directmetal::d3d11 {

// The first level of `requested` the device supports, or of Direct3D's default list when it is
// empty. E_INVALIDARG when a requested value is not a feature level, DXGI_ERROR_UNSUPPORTED when
// none is supported.
HRESULT negotiate_feature_level(std::span<const D3D_FEATURE_LEVEL> requested,
                                D3D_FEATURE_LEVEL& chosen) noexcept;

} // namespace directmetal::d3d11
