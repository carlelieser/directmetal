#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/formats/formats.hpp>

#include <d3d11.h>

namespace directmetal::d3d11 {

// ClearRenderTargetView's color for `format`: integer formats take each float truncated toward
// zero and saturated to 32 bits (NaN becomes 0), others the float bits.
backend::ClearColor to_clear_color(const FLOAT color[4],
                                   const formats::FormatInfo& format) noexcept;

} // namespace directmetal::d3d11
