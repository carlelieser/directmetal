#pragma once

#include "context_query_stubs.hpp"
#include "device_child.hpp"

namespace directmetal::d3d11 {

class Texture2D;

// The immediate context. Owned by its device through a private reference; it records into the
// device's stream and holds no unix-side objects of its own.
class DeviceContext final : public DeviceChild<ContextQueryStubs> {
public:
    // Starts with one public reference.
    explicit DeviceContext(Device& device) noexcept;

    HRESULT STDMETHODCALLTYPE Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType,
                                  UINT MapFlags,
                                  D3D11_MAPPED_SUBRESOURCE* pMappedResource) noexcept override;
    void STDMETHODCALLTYPE Unmap(ID3D11Resource* pResource, UINT Subresource) noexcept override;
    void STDMETHODCALLTYPE CopyResource(ID3D11Resource* pDstResource,
                                        ID3D11Resource* pSrcResource) noexcept override;
    void STDMETHODCALLTYPE ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView,
                                                 const FLOAT ColorRGBA[4]) noexcept override;
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() noexcept override;

protected:
    void* find_interface(REFIID iid) noexcept override;

private:
    ~DeviceContext() override;

    // This device's texture, or null.
    Texture2D* own_texture(ID3D11Resource* resource) const noexcept;
    // Waits until the GPU has finished writing the texture, or with D3D11_MAP_FLAG_DO_NOT_WAIT
    // returns DXGI_ERROR_WAS_STILL_DRAWING if it has not.
    HRESULT wait_for_writes(const Texture2D& texture, UINT map_flags);
    void copy_resource(Texture2D& destination, const Texture2D& source);
};

} // namespace directmetal::d3d11
