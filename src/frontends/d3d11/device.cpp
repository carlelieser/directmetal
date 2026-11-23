#include "device.hpp"

#include "device_context.hpp"
#include "hresult.hpp"
#include "render_target_view.hpp"
#include "texture.hpp"
#include "texture_desc.hpp"

#include <directmetal/com/boundary.hpp>

namespace directmetal::d3d11 {

void retain_device(Device& device) noexcept {
    device.AddRef();
}

void release_device(Device& device) noexcept {
    device.Release();
}

HRESULT Device::create(std::unique_ptr<BridgeDevice> bridge, Device*& device) {
    auto* created = new Device(std::move(bridge));
    DeviceContext* context = nullptr;
    try {
        context = new DeviceContext(*created);
    } catch (...) {
        created->Release();
        throw;
    }
    created->immediate_ = com::PrivateRef<DeviceContext>(context);
    // Direct3D's immediate context has no public reference until GetImmediateContext.
    context->Release();
    device = created;
    return S_OK;
}

Device::Device(std::unique_ptr<BridgeDevice> bridge) noexcept : bridge_(std::move(bridge)) {}

Device::~Device() = default;

HRESULT STDMETHODCALLTYPE Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc,
                                                  const D3D11_SUBRESOURCE_DATA* pInitialData,
                                                  ID3D11Texture2D** ppTexture2D) noexcept {
    DM_IMPL("ID3D11Device::CreateTexture2D");
    if (ppTexture2D != nullptr) {
        *ppTexture2D = nullptr;
    }
    if (pDesc == nullptr) {
        return E_INVALIDARG;
    }
    if (pInitialData != nullptr) {
        return not_implemented("ID3D11Device::CreateTexture2D", "initial data");
    }
    return com::guard("ID3D11Device::CreateTexture2D",
                      [&] { return create_texture2d(*pDesc, ppTexture2D); });
}

HRESULT STDMETHODCALLTYPE Device::CreateRenderTargetView(
    ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
    ID3D11RenderTargetView** ppRTView) noexcept {
    DM_IMPL("ID3D11Device::CreateRenderTargetView");
    if (ppRTView != nullptr) {
        *ppRTView = nullptr;
    }
    return com::guard("ID3D11Device::CreateRenderTargetView",
                      [&] { return create_render_target_view(pResource, pDesc, ppRTView); });
}

HRESULT STDMETHODCALLTYPE Device::GetPrivateData(REFGUID guid, UINT* pDataSize,
                                                 void* pData) noexcept {
    DM_IMPL("ID3D11Device::GetPrivateData");
    if (pDataSize == nullptr) {
        return E_INVALIDARG;
    }
    return com::guard("ID3D11Device::GetPrivateData",
                      [&] { return to_hresult(private_data_.get(guid, pDataSize, pData)); });
}

HRESULT STDMETHODCALLTYPE Device::SetPrivateData(REFGUID guid, UINT DataSize,
                                                 const void* pData) noexcept {
    DM_IMPL("ID3D11Device::SetPrivateData");
    return com::guard("ID3D11Device::SetPrivateData",
                      [&] { return to_hresult(private_data_.set(guid, DataSize, pData)); });
}

HRESULT STDMETHODCALLTYPE Device::SetPrivateDataInterface(REFGUID guid,
                                                          const IUnknown* pData) noexcept {
    DM_IMPL("ID3D11Device::SetPrivateDataInterface");
    // The store takes a reference, which needs a non-const pointer.
    auto* object = const_cast<IUnknown*>(pData);
    return com::guard("ID3D11Device::SetPrivateDataInterface",
                      [&] { return to_hresult(private_data_.set_interface(guid, object)); });
}

void STDMETHODCALLTYPE
Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) noexcept {
    DM_IMPL("ID3D11Device::GetImmediateContext");
    if (ppImmediateContext == nullptr) {
        return;
    }
    immediate_->AddRef();
    *ppImmediateContext = immediate_.get();
}

BridgeDevice& Device::bridge() const noexcept {
    return *bridge_;
}

void* Device::find_interface(REFIID iid) noexcept {
    return com::find_in_table(com::interface_table<Device, IUnknown, ID3D11Device>(), iid, this);
}

HRESULT Device::create_texture2d(const D3D11_TEXTURE2D_DESC& desc, ID3D11Texture2D** texture) {
    TexturePlan plan{};
    if (const HRESULT result = plan_texture2d(desc, bridge_->features(), plan); FAILED(result)) {
        return result;
    }
    if (texture == nullptr) {
        return S_FALSE;
    }
    Texture2D* created = nullptr;
    const HRESULT result = Texture2D::create(*this, plan, created);
    if (SUCCEEDED(result)) {
        *texture = created;
    }
    return result;
}

HRESULT Device::create_render_target_view(ID3D11Resource* resource,
                                          const D3D11_RENDER_TARGET_VIEW_DESC* desc,
                                          ID3D11RenderTargetView** view) {
    Texture2D* texture = find_own<Texture2D>(resource);
    if (texture == nullptr || &texture->device() != this) {
        return E_INVALIDARG;
    }
    RenderTargetPlan plan{};
    if (const HRESULT result = plan_render_target_view(*texture, desc, plan); FAILED(result)) {
        return result;
    }
    if (view == nullptr) {
        return S_FALSE;
    }
    *view = new RenderTargetView(*texture, plan);
    return S_OK;
}

} // namespace directmetal::d3d11
