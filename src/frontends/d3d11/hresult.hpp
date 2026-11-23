#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/com/private_data.hpp>

#include <d3d11.h>

#include <string_view>

namespace directmetal::d3d11 {

// backend::Status::unsupported has no single Direct3D meaning, so each call site names its
// result: DXGI_ERROR_UNSUPPORTED for device creation, E_INVALIDARG or E_OUTOFMEMORY for resources.
HRESULT to_hresult(backend::Status status, HRESULT unsupported) noexcept;
HRESULT to_hresult(com::PrivateDataResult result) noexcept;

// For a valid request that needs more than the implemented slice: logs operation and feature and
// returns E_NOTIMPL, never a success code.
HRESULT not_implemented(std::string_view operation, std::string_view feature) noexcept;

// Logs a call Direct3D ignores because its arguments are invalid.
void report_ignored_call(std::string_view operation, std::string_view reason) noexcept;

} // namespace directmetal::d3d11
