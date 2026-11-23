#pragma once

#include "device_child.hpp"

#include <directmetal/backend/types.hpp>
#include <directmetal/com/ref.hpp>

namespace directmetal::d3d11 {

class Texture2D;

// A view description that passed validation, with the subresources it covers.
struct RenderTargetPlan {
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    backend::SubresourceRange range;
};

// E_INVALIDARG for views Direct3D rejects, E_NOTIMPL for a format other than the texture's. A null
// desc views mip 0 of every slice in the texture's format.
HRESULT plan_render_target_view(const Texture2D& texture, const D3D11_RENDER_TARGET_VIEW_DESC* desc,
                                RenderTargetPlan& plan) noexcept;

// Keeps its texture alive through a private reference, so releasing the texture's public
// references while the view exists does not destroy it.
class RenderTargetView final : public DeviceChild<ID3D11RenderTargetView> {
public:
    static constexpr GUID identity = {
        0xb67874cc, 0x0de2, 0x449c, {0x85, 0xfe, 0xff, 0x46, 0x57, 0xe2, 0x2e, 0x4a}};

    // Starts with one public reference.
    RenderTargetView(Texture2D& texture, const RenderTargetPlan& plan) noexcept;

    void STDMETHODCALLTYPE GetResource(ID3D11Resource**) noexcept override;
    void STDMETHODCALLTYPE GetDesc(D3D11_RENDER_TARGET_VIEW_DESC*) noexcept override;

    Texture2D& texture() const noexcept;
    const backend::SubresourceRange& range() const noexcept;

protected:
    void* find_interface(REFIID iid) noexcept override;

private:
    ~RenderTargetView() override;

    com::PrivateRef<Device> owner_;
    com::PrivateRef<Texture2D> texture_;
    RenderTargetPlan plan_;
};

} // namespace directmetal::d3d11
