#include "texture.hpp"

#include "bridge_device.hpp"
#include "device.hpp"
#include "hresult.hpp"

#include <directmetal/bridge/client.hpp>
#include <directmetal/runtime/log.hpp>

#include <new>
#include <span>

namespace directmetal::d3d11 {

HRESULT Texture2D::create(Device& device, const TexturePlan& plan, Texture2D*& texture) {
    std::optional<Staging> staging;
    if (d3d11::is_staging(plan.desc)) {
        staging.emplace(Staging{StagingLayout(plan.desc, *plan.format), nullptr});
    }
    auto* created = new Texture2D(device, plan, std::move(staging));
    const HRESULT result = created->create_unix_object(device.bridge(), plan);
    if (FAILED(result)) {
        created->Release();
        return result;
    }
    texture = created;
    return S_OK;
}

Texture2D::Texture2D(Device& device, const TexturePlan& plan,
                     std::optional<Staging> staging) noexcept
    : DeviceChild(device), owner_(&device), desc_(plan.desc), format_(plan.format), handle_{},
      staging_(std::move(staging)) {}

HRESULT Texture2D::create_unix_object(BridgeDevice& bridge, const TexturePlan& plan) {
    if (!staging_.has_value()) {
        auto created = bridge.create_texture(to_backend_desc(plan));
        handle_ = created.has_value() ? created.value() : bridge::Handle{};
        return to_hresult(created.status(), E_INVALIDARG);
    }
    const std::uint64_t size = staging_->layout.size();
    auto* memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(size));
    if (memory == nullptr) {
        return E_OUTOFMEMORY;
    }
    auto imported = bridge.import_memory(std::span(memory, size));
    if (!imported.has_value()) {
        directmetal_bridge_free_shared(memory, size);
        return to_hresult(imported.status(), E_OUTOFMEMORY);
    }
    staging_->memory = memory;
    handle_ = imported.value();
    return S_OK;
}

Texture2D::~Texture2D() {
    if (bridge::is_null(handle_)) {
        return;
    }
    BridgeDevice& bridge = device().bridge();
    if (!staging_.has_value()) {
        try {
            bridge.record(bridge::DestroyObjectCommand{handle_});
        } catch (const std::bad_alloc&) {
            // destroy_device destroys every object of the device, so the texture is only kept
            // until then.
            log::print(log::Component::d3d11, log::Level::error,
                       "Texture2D: out of memory recording the destruction of texture {}; it is "
                       "kept until its device is destroyed",
                       handle_.value);
        }
        return;
    }
    if (bridge.release_memory(handle_) != backend::Status::success) {
        // The unix side may still use the memory, so it is never freed.
        log::print(log::Component::d3d11, log::Level::error,
                   "Texture2D: release_memory failed for buffer {}; leaking {} bytes",
                   handle_.value, staging_->layout.size());
        return;
    }
    directmetal_bridge_free_shared(staging_->memory, staging_->layout.size());
}

void STDMETHODCALLTYPE Texture2D::GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) noexcept {
    DM_IMPL("ID3D11Resource::GetType");
    if (pResourceDimension != nullptr) {
        *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
    }
}

void STDMETHODCALLTYPE Texture2D::SetEvictionPriority(UINT) noexcept {
    DM_STUB_VOID("ID3D11Resource::SetEvictionPriority");
}

UINT STDMETHODCALLTYPE Texture2D::GetEvictionPriority() noexcept {
    DM_STUB_RETURN("ID3D11Resource::GetEvictionPriority", 0);
}

void STDMETHODCALLTYPE Texture2D::GetDesc(D3D11_TEXTURE2D_DESC* pDesc) noexcept {
    DM_IMPL("ID3D11Texture2D::GetDesc");
    if (pDesc != nullptr) {
        *pDesc = desc_;
    }
}

const D3D11_TEXTURE2D_DESC& Texture2D::desc() const noexcept {
    return desc_;
}

const formats::FormatInfo& Texture2D::format() const noexcept {
    return *format_;
}

bridge::Handle Texture2D::handle() const noexcept {
    return handle_;
}

bool Texture2D::is_staging() const noexcept {
    return staging_.has_value();
}

UINT Texture2D::subresource_count() const noexcept {
    return desc_.MipLevels * desc_.ArraySize;
}

const StagingLayout& Texture2D::layout() const noexcept {
    return staging_->layout;
}

std::byte* Texture2D::memory() const noexcept {
    return staging_->memory;
}

std::uint64_t Texture2D::last_write() const noexcept {
    return last_write_.load(std::memory_order_acquire);
}

void Texture2D::set_last_write(std::uint64_t value) noexcept {
    last_write_.store(value, std::memory_order_release);
}

void* Texture2D::find_interface(REFIID iid) noexcept {
    if (IsEqualGUID(iid, identity)) {
        return this;
    }
    return com::find_in_table(com::interface_table<Texture2D, IUnknown, ID3D11DeviceChild,
                                                   ID3D11Resource, ID3D11Texture2D>(),
                              iid, this);
}

} // namespace directmetal::d3d11
