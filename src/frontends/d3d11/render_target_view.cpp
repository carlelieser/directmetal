#include "render_target_view.hpp"

#include "device.hpp"
#include "hresult.hpp"
#include "texture.hpp"

#include <cstdint>

namespace directmetal::d3d11 {
namespace {

bool is_span_within(UINT first, UINT count, UINT limit) noexcept {
    return count >= 1 && first < limit && count <= limit - first;
}

// The view's subresources, or E_INVALIDARG for a dimension or range the texture cannot have.
HRESULT plan_range(const D3D11_TEXTURE2D_DESC& texture, const D3D11_RENDER_TARGET_VIEW_DESC& desc,
                   backend::SubresourceRange& range) noexcept {
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) {
        const bool is_valid = desc.Texture2D.MipSlice < texture.MipLevels && texture.ArraySize == 1;
        range = {desc.Texture2D.MipSlice, 1, 0, 1};
        return is_valid ? S_OK : E_INVALIDARG;
    }
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY) {
        const D3D11_TEX2D_ARRAY_RTV& array = desc.Texture2DArray;
        const bool is_mip_valid = array.MipSlice < texture.MipLevels;
        range = {array.MipSlice, 1, array.FirstArraySlice, array.ArraySize};
        const bool is_slices_valid =
            is_span_within(array.FirstArraySlice, array.ArraySize, texture.ArraySize);
        return is_mip_valid && is_slices_valid ? S_OK : E_INVALIDARG;
    }
    return E_INVALIDARG;
}

D3D11_RENDER_TARGET_VIEW_DESC whole_texture_view(const D3D11_TEXTURE2D_DESC& texture) noexcept {
    D3D11_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = texture.Format;
    if (texture.ArraySize == 1) {
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    } else {
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.ArraySize = texture.ArraySize;
    }
    return desc;
}

HRESULT check_format(const D3D11_TEXTURE2D_DESC& texture, DXGI_FORMAT view) noexcept {
    if (view == texture.Format) {
        return S_OK;
    }
    const bool is_compatible =
        formats::is_view_compatible(static_cast<std::uint32_t>(texture.Format),
                                    static_cast<std::uint32_t>(view), formats::CastingRules::d3d11);
    return is_compatible ? not_implemented("ID3D11Device::CreateRenderTargetView",
                                           "a view format other than the resource's")
                         : E_INVALIDARG;
}

} // namespace

HRESULT plan_render_target_view(const Texture2D& texture, const D3D11_RENDER_TARGET_VIEW_DESC* desc,
                                RenderTargetPlan& plan) noexcept {
    const D3D11_TEXTURE2D_DESC& texture_desc = texture.desc();
    if ((texture_desc.BindFlags & D3D11_BIND_RENDER_TARGET) == 0) {
        return E_INVALIDARG;
    }
    plan.desc = desc == nullptr ? whole_texture_view(texture_desc) : *desc;
    if (plan.desc.Format == DXGI_FORMAT_UNKNOWN) {
        plan.desc.Format = texture_desc.Format;
    }
    if (const HRESULT result = plan_range(texture_desc, plan.desc, plan.range); FAILED(result)) {
        return result;
    }
    return check_format(texture_desc, plan.desc.Format);
}

RenderTargetView::RenderTargetView(Texture2D& texture, const RenderTargetPlan& plan) noexcept
    : DeviceChild(texture.device()), owner_(&texture.device()), texture_(&texture), plan_(plan) {}

RenderTargetView::~RenderTargetView() = default;

void STDMETHODCALLTYPE RenderTargetView::GetResource(ID3D11Resource** ppResource) noexcept {
    DM_IMPL("ID3D11View::GetResource");
    if (ppResource != nullptr) {
        texture_->AddRef();
        *ppResource = texture_.get();
    }
}

void STDMETHODCALLTYPE RenderTargetView::GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) noexcept {
    DM_IMPL("ID3D11RenderTargetView::GetDesc");
    if (pDesc != nullptr) {
        *pDesc = plan_.desc;
    }
}

Texture2D& RenderTargetView::texture() const noexcept {
    return *texture_.get();
}

const backend::SubresourceRange& RenderTargetView::range() const noexcept {
    return plan_.range;
}

void* RenderTargetView::find_interface(REFIID iid) noexcept {
    if (IsEqualGUID(iid, identity)) {
        return this;
    }
    return com::find_in_table(com::interface_table<RenderTargetView, IUnknown, ID3D11DeviceChild,
                                                   ID3D11View, ID3D11RenderTargetView>(),
                              iid, this);
}

} // namespace directmetal::d3d11
