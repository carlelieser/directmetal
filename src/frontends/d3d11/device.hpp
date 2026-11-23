#pragma once

#include "bridge_device.hpp"
#include "device_stubs.hpp"

#include <directmetal/com/object.hpp>
#include <directmetal/com/private_data.hpp>
#include <directmetal/com/ref.hpp>

#include <memory>

namespace directmetal::d3d11 {

class DeviceContext;

// Ownership: the device owns its BridgeDevice and immediate context. Children (resources, views,
// the context) hold a public device reference while they have public references; resources and
// views also hold a private one, so the device and its stream outlive every unix-side object they
// still have to destroy. The device is destroyed, and its unix-side device with it, once both
// counts reach zero.
class Device final : public com::Object<DeviceStubs> {
public:
    // The device starts with one public reference; its immediate context has none.
    static HRESULT create(std::unique_ptr<BridgeDevice> bridge, Device*& device);

    HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc,
                                              const D3D11_SUBRESOURCE_DATA* pInitialData,
                                              ID3D11Texture2D** ppTexture2D) noexcept override;
    HRESULT STDMETHODCALLTYPE
    CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
                           ID3D11RenderTargetView** ppRTView) noexcept override;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize,
                                             void* pData) noexcept override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                             const void* pData) noexcept override;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid,
                                                      const IUnknown* pData) noexcept override;
    void STDMETHODCALLTYPE
    GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) noexcept override;

    BridgeDevice& bridge() const noexcept;

protected:
    void* find_interface(REFIID iid) noexcept override;

private:
    explicit Device(std::unique_ptr<BridgeDevice> bridge) noexcept;
    ~Device() override;

    HRESULT create_texture2d(const D3D11_TEXTURE2D_DESC& desc, ID3D11Texture2D** texture);
    HRESULT create_render_target_view(ID3D11Resource* resource,
                                      const D3D11_RENDER_TARGET_VIEW_DESC* desc,
                                      ID3D11RenderTargetView** view);

    // Declared first, destroyed last: the context and private data go before the unix device.
    std::unique_ptr<BridgeDevice> bridge_;
    com::PrivateDataStore private_data_;
    com::PrivateRef<DeviceContext> immediate_;
};

} // namespace directmetal::d3d11
