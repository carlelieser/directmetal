#pragma once

#include <directmetal/com/object.hpp>
#include <directmetal/frontends/markers.hpp>

#include <d3d11.h>

namespace directmetal::d3d11 {

class Device;

// Public references on the device, taken by children while they have public references.
void retain_device(Device& device) noexcept;
void release_device(Device& device) noexcept;

// The object answering `object`'s private identity IID, which no application interface has, or
// null. The caller's reference keeps it alive.
template <typename Own> Own* find_own(IUnknown* object) noexcept {
    void* found = nullptr;
    if (object == nullptr || FAILED(object->QueryInterface(Own::identity, &found))) {
        return nullptr;
    }
    Own* own = static_cast<Own*>(found);
    own->Release();
    return own;
}

// Base of objects implementing an ID3D11DeviceChild interface. While the child has public
// references it holds one public reference on its device, so the device's count includes its
// children, as in Direct3D.
template <typename Base> class DeviceChild : public com::Object<Base> {
public:
    void STDMETHODCALLTYPE GetDevice(ID3D11Device**) noexcept override {
        DM_STUB_VOID("ID3D11DeviceChild::GetDevice");
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) noexcept override {
        DM_STUB("ID3D11DeviceChild::GetPrivateData");
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) noexcept override {
        DM_STUB("ID3D11DeviceChild::SetPrivateData");
    }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) noexcept override {
        DM_STUB("ID3D11DeviceChild::SetPrivateDataInterface");
    }

    Device& device() const noexcept { return device_; }

protected:
    // The child's first public reference holds a device reference too.
    explicit DeviceChild(Device& device) noexcept : device_(device) { retain_device(device_); }

    void on_first_public_ref() noexcept override { retain_device(device_); }
    void on_last_public_release() noexcept override { release_device(device_); }

private:
    Device& device_;
};

} // namespace directmetal::d3d11
