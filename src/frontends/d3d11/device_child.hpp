#pragma once

#include "hresult.hpp"

#include <directmetal/com/boundary.hpp>
#include <directmetal/com/object.hpp>
#include <directmetal/com/private_data.hpp>
#include <directmetal/frontends/markers.hpp>

#include <d3d11.h>

namespace directmetal::d3d11 {

class Device;

// Public references on the device, taken by children while they have public references.
void retain_device(Device& device) noexcept;
void release_device(Device& device) noexcept;
ID3D11Device* device_interface(Device& device) noexcept;

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
    void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) noexcept override {
        DM_IMPL("ID3D11DeviceChild::GetDevice");
        if (ppDevice != nullptr) {
            *ppDevice = device_interface(device_);
            (*ppDevice)->AddRef();
        }
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize,
                                             void* pData) noexcept override {
        DM_IMPL("ID3D11DeviceChild::GetPrivateData");
        if (pDataSize == nullptr) {
            return E_INVALIDARG;
        }
        return com::guard("ID3D11DeviceChild::GetPrivateData",
                          [&] { return to_hresult(private_data_.get(guid, pDataSize, pData)); });
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                             const void* pData) noexcept override {
        DM_IMPL("ID3D11DeviceChild::SetPrivateData");
        return com::guard("ID3D11DeviceChild::SetPrivateData",
                          [&] { return to_hresult(private_data_.set(guid, DataSize, pData)); });
    }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid,
                                                      const IUnknown* pData) noexcept override {
        DM_IMPL("ID3D11DeviceChild::SetPrivateDataInterface");
        // The store takes a reference, which needs a non-const pointer.
        auto* object = const_cast<IUnknown*>(pData);
        return com::guard("ID3D11DeviceChild::SetPrivateDataInterface",
                          [&] { return to_hresult(private_data_.set_interface(guid, object)); });
    }

    Device& device() const noexcept { return device_; }

protected:
    // The child's first public reference holds a device reference too.
    explicit DeviceChild(Device& device) noexcept : device_(device) { retain_device(device_); }

    void on_first_public_ref() noexcept override { retain_device(device_); }
    void on_last_public_release() noexcept override { release_device(device_); }

private:
    Device& device_;
    com::PrivateDataStore private_data_;
};

} // namespace directmetal::d3d11
