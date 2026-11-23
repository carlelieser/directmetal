#pragma once

#include <d3d11.h>

#include <span>

namespace directmetal::d3d11 {

// The first level of `requested` the device supports (11_0 only), or of Direct3D's default list
// when it is empty. E_INVALIDARG when a requested value is not a feature level or when none is
// supported and 11_1 was requested, DXGI_ERROR_UNSUPPORTED when none is supported otherwise.
HRESULT negotiate_feature_level(std::span<const D3D_FEATURE_LEVEL> requested,
                                D3D_FEATURE_LEVEL& chosen) noexcept;

} // namespace directmetal::d3d11
