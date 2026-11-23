#include "device_context.hpp"

#include "bridge_device.hpp"
#include "clear_color.hpp"
#include "context_commands.hpp"
#include "device.hpp"
#include "hresult.hpp"
#include "render_target_view.hpp"
#include "texture.hpp"

#include <directmetal/com/boundary.hpp>

#include <algorithm>
#include <array>
#include <chrono>

namespace directmetal::d3d11 {
namespace {

struct MapRule {
    D3D11_MAP type;
    // CPU access flags the resource needs; 0 for types STAGING resources never accept.
    UINT cpu_access;
};

// https://learn.microsoft.com/windows/win32/api/d3d11/ne-d3d11-d3d11_map
constexpr std::array staging_map_rules = {
    MapRule{D3D11_MAP_READ, D3D11_CPU_ACCESS_READ},
    MapRule{D3D11_MAP_WRITE, D3D11_CPU_ACCESS_WRITE},
    MapRule{D3D11_MAP_READ_WRITE, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE},
    MapRule{D3D11_MAP_WRITE_DISCARD, 0},
    MapRule{D3D11_MAP_WRITE_NO_OVERWRITE, 0},
};

bool is_valid_map(const Texture2D& texture, UINT subresource, D3D11_MAP type) noexcept {
    const auto rule = std::find_if(staging_map_rules.begin(), staging_map_rules.end(),
                                   [type](const MapRule& item) { return item.type == type; });
    if (rule == staging_map_rules.end() || rule->cpu_access == 0) {
        return false;
    }
    const bool has_access = (texture.desc().CPUAccessFlags & rule->cpu_access) == rule->cpu_access;
    return has_access && subresource < texture.subresource_count();
}

D3D11_MAPPED_SUBRESOURCE mapping_of(const Texture2D& texture, UINT subresource) noexcept {
    const SubresourceLayout& layout = texture.layout().at(subresource);
    return {texture.memory() + layout.offset, layout.row_pitch, layout.depth_pitch};
}

} // namespace

DeviceContext::DeviceContext(Device& device) noexcept : DeviceChild(device) {}

DeviceContext::~DeviceContext() = default;

HRESULT STDMETHODCALLTYPE DeviceContext::Map(ID3D11Resource* pResource, UINT Subresource,
                                             D3D11_MAP MapType, UINT MapFlags,
                                             D3D11_MAPPED_SUBRESOURCE* pMappedResource) noexcept {
    DM_IMPL("ID3D11DeviceContext::Map");
    if (pMappedResource != nullptr) {
        *pMappedResource = {};
    }
    const Texture2D* texture = own_texture(pResource);
    const bool is_mappable = texture != nullptr && texture->is_staging();
    if (!is_mappable || (MapFlags & ~D3D11_MAP_FLAG_DO_NOT_WAIT) != 0) {
        return E_INVALIDARG;
    }
    if (!is_valid_map(*texture, Subresource, MapType) || pMappedResource == nullptr) {
        return E_INVALIDARG;
    }
    return com::guard("ID3D11DeviceContext::Map", [&] {
        const HRESULT result = wait_for_writes(*texture, MapFlags);
        if (SUCCEEDED(result)) {
            *pMappedResource = mapping_of(*texture, Subresource);
        }
        return result;
    });
}

void STDMETHODCALLTYPE DeviceContext::Unmap(ID3D11Resource* pResource, UINT Subresource) noexcept {
    DM_IMPL("ID3D11DeviceContext::Unmap");
    // Staging memory stays mapped and coherent, so there is nothing to write back.
    const Texture2D* texture = own_texture(pResource);
    const bool is_mapped_kind = texture != nullptr && texture->is_staging();
    if (!is_mapped_kind || Subresource >= texture->subresource_count()) {
        report_ignored_call("ID3D11DeviceContext::Unmap", "not a mappable subresource");
    }
}

void STDMETHODCALLTYPE DeviceContext::CopyResource(ID3D11Resource* pDstResource,
                                                   ID3D11Resource* pSrcResource) noexcept {
    DM_IMPL("ID3D11DeviceContext::CopyResource");
    Texture2D* destination = own_texture(pDstResource);
    const Texture2D* source = own_texture(pSrcResource);
    const bool is_pair = destination != nullptr && source != nullptr && destination != source;
    if (!is_pair || !is_copy_compatible(destination->desc(), source->desc())) {
        report_ignored_call("ID3D11DeviceContext::CopyResource", "resources are not copyable");
        return;
    }
    com::guard("ID3D11DeviceContext::CopyResource", [&] {
        copy_resource(*destination, *source);
        return S_OK;
    });
}

void STDMETHODCALLTYPE DeviceContext::ClearRenderTargetView(
    ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4]) noexcept {
    DM_IMPL("ID3D11DeviceContext::ClearRenderTargetView");
    const RenderTargetView* view = find_own<RenderTargetView>(pRenderTargetView);
    const bool is_own_view = view != nullptr && &view->device() == &device();
    if (!is_own_view || ColorRGBA == nullptr) {
        report_ignored_call("ID3D11DeviceContext::ClearRenderTargetView", "invalid arguments");
        return;
    }
    const Texture2D& texture = view->texture();
    const bridge::ClearTextureCommand command{texture.handle(), view->range(),
                                              to_clear_color(ColorRGBA, texture.format()), 0};
    com::guard("ID3D11DeviceContext::ClearRenderTargetView", [&] {
        device().bridge().record(command);
        return S_OK;
    });
}

D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE DeviceContext::GetType() noexcept {
    DM_IMPL("ID3D11DeviceContext::GetType");
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
}

void* DeviceContext::find_interface(REFIID iid) noexcept {
    return com::find_in_table(
        com::interface_table<DeviceContext, IUnknown, ID3D11DeviceChild, ID3D11DeviceContext>(),
        iid, this);
}

Texture2D* DeviceContext::own_texture(ID3D11Resource* resource) const noexcept {
    Texture2D* texture = find_own<Texture2D>(resource);
    return texture != nullptr && &texture->device() == &device() ? texture : nullptr;
}

HRESULT DeviceContext::wait_for_writes(const Texture2D& texture, UINT map_flags) {
    const bool is_polling = (map_flags & D3D11_MAP_FLAG_DO_NOT_WAIT) != 0;
    const auto timeout =
        is_polling ? std::chrono::nanoseconds(-1) : std::chrono::nanoseconds::max();
    return to_hresult(device().bridge().wait(texture.last_write(), timeout));
}

void DeviceContext::copy_resource(Texture2D& destination, const Texture2D& source) {
    if (!destination.is_staging() || source.is_staging()) {
        not_implemented("ID3D11DeviceContext::CopyResource",
                        "copies other than DEFAULT to STAGING");
        return;
    }
    destination.set_last_write(record_readback(device().bridge(), destination, source));
}

} // namespace directmetal::d3d11
