#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/formats/formats.hpp>

#include <d3d11.h>

namespace directmetal::d3d11 {

// A CreateTexture2D description that passed validation.
struct TexturePlan {
    // MipLevels is resolved: 0 became the full chain.
    D3D11_TEXTURE2D_DESC desc;
    const formats::FormatInfo* format;
};

// E_INVALIDARG for descriptions Direct3D rejects, E_NOTIMPL for valid ones outside the implemented
// slice (DEFAULT render targets and STAGING textures with CPU read access), E_OUTOFMEMORY for
// textures larger than StagingLayout::max_resource_bytes.
HRESULT plan_texture2d(const D3D11_TEXTURE2D_DESC& desc, const formats::DeviceFeatures& features,
                       TexturePlan& plan) noexcept;

bool is_staging(const D3D11_TEXTURE2D_DESC& desc) noexcept;

// The Metal texture of a DEFAULT texture.
backend::TextureDesc to_backend_desc(const TexturePlan& plan) noexcept;

} // namespace directmetal::d3d11
