#pragma once

#include <unknwn.h>

#include <cstdint>
#include <span>

// COM object base. QueryInterface is driven by each class's interface table; reference counts are
// split into public (AddRef/Release) and private (internal owners such as a device's children).
// Threading: counts are atomic, QueryInterface is lock-free.
namespace directmetal::com {

template <typename Interface> const GUID& iid_of() noexcept {
    return __uuidof(Interface);
}

struct InterfaceEntry {
    const GUID* iid;
    void* (*cast)(void* self) noexcept;
};

template <typename Self, typename Interface> void* cast_to(void* self) noexcept {
    return static_cast<Interface*>(static_cast<Self*>(self));
}

// Every IID the class answers, ancestors included, e.g.
// interface_table<Device, IUnknown, ID3D11Device, ID3D11Device1>().
template <typename Self, typename... Interfaces>
std::span<const InterfaceEntry> interface_table() noexcept {
    static const InterfaceEntry entries[] = {
        {&iid_of<Interfaces>(), &cast_to<Self, Interfaces>}...};
    return entries;
}

class RefCounts {
public:
    ULONG add_ref() noexcept;
    // Returns the new public count.
    ULONG release() noexcept;
    void add_private_ref() noexcept;
    // Returns true when both counts reached zero and the object must be destroyed.
    bool release_private() noexcept;
};

// Base implements the COM interfaces; Extra are further interfaces of the same object.
template <typename Base, typename... Extra> class Object : public Base, public Extra... {
public:
    // E_POINTER for a null out-pointer, E_NOINTERFACE (and *object = nullptr) when unsupported.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override;
    ULONG STDMETHODCALLTYPE AddRef() noexcept override;
    ULONG STDMETHODCALLTYPE Release() noexcept override;

    void add_private_ref() noexcept;
    void release_private() noexcept;

protected:
    Object() noexcept;
    virtual ~Object();
    // The derived class's interface table lookup.
    virtual void* find_interface(REFIID iid) noexcept = 0;
    // Runs when the public count reaches zero; children drop their device reference here.
    virtual void on_last_public_release() noexcept {}
};

} // namespace directmetal::com
