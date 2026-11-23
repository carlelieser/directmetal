#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/formats/formats.hpp>

#include <d3d11.h>

#include <cstdint>

namespace directmetal::d3d11 {

class BridgeDevice;
class Texture2D;

// CopyResource's requirements: equal dimensions, mip levels, array size and sampling, and the same
// format or formats of one typeless family.
bool is_copy_compatible(const D3D11_TEXTURE2D_DESC& destination,
                        const D3D11_TEXTURE2D_DESC& source) noexcept;

// Records a copy of every subresource of a DEFAULT texture into a STAGING texture; returns the
// progress value whose completion finishes it.
std::uint64_t record_readback(BridgeDevice& bridge, const Texture2D& destination,
                              const Texture2D& source);

} // namespace directmetal::d3d11
