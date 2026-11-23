#pragma once

#include <directmetal/frontends/markers.hpp>

#include <unknwn.h>

#include <atomic>
#include <cstdint>
#include <span>
#include <type_traits>

// COM object base. QueryInterface is driven by each class's interface table; reference counts are
// split into public (AddRef/Release) and private (internal owners such as a device's children).
// Threading: counts are atomic, QueryInterface is lock-free.
// ABI: objects built natively use the Itanium C++ ABI, not MSVC's, so PE code never calls them.
namespace directmetal::com {

template <typename Base, typename... Extra> class Object;

template <typename Interface> const GUID& iid_of() noexcept {
    return __uuidof(Interface);
}

struct InterfaceEntry {
    const GUID* iid;
    void* (*cast)(void* self) noexcept;
};

// An interface that Base and an Extra interface both derive from, IUnknown at least, resolves
// through Base, so every IID maps to one pointer.
template <typename Interface, typename Base, typename... Extra>
Interface* to_interface(Object<Base, Extra...>* object) noexcept {
    if constexpr (std::is_base_of_v<Interface, Base>) {
        return static_cast<Base*>(object);
    } else {
        return object;
    }
}

template <typename Self, typename Interface> void* cast_to(void* self) noexcept {
    return to_interface<Interface>(static_cast<Self*>(self));
}

// Every IID the class answers, ancestors included, e.g.
// interface_table<Device, IUnknown, ID3D11Device, ID3D11Device1>().
template <typename Self, typename... Interfaces>
std::span<const InterfaceEntry> interface_table() noexcept {
    static const InterfaceEntry entries[] = {
        {&iid_of<Interfaces>(), &cast_to<Self, Interfaces>}...};
    return entries;
}

// The entry of `table` matching `iid`, cast from `self`, or null; implements find_interface.
template <typename Self>
void* find_in_table(std::span<const InterfaceEntry> table, REFIID iid, Self* self) noexcept {
    for (const InterfaceEntry& entry : table) {
        if (IsEqualGUID(*entry.iid, iid)) {
            return entry.cast(self);
        }
    }
    return nullptr;
}

// Public references (AddRef/Release) and private references (internal owners such as a device's
// children). While the public count is non-zero the public references together hold one private
// reference; the object is destroyed when the private count reaches zero. A new object starts with
// one public reference.
class RefCounts {
public:
    // Returns the new public count. Going from 0 to 1 takes the private reference.
    ULONG add_ref() noexcept;
    // Returns the new public count. Going from 1 to 0 hands the public references' private
    // reference to the caller, which drops it with release_private.
    ULONG release() noexcept;
    void add_private_ref() noexcept;
    // Returns true when the private count reached zero and the object must be destroyed.
    bool release_private() noexcept;

private:
    std::atomic<ULONG> public_count_{1};
    std::atomic<ULONG> private_count_{1};
};

// Base implements the COM interfaces; Extra are further interfaces of the same object. IUnknown
// resolves through Base: the overrides below serve every base's IUnknown slots.
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
    // The hooks below follow the count transitions without a lock: on another thread a hook may
    // run concurrently with, or before, the hook of an earlier transition. Each must be
    // thread-safe and a balanced, commutative operation such as taking or dropping a reference; a
    // hook that needs ordered state serializes itself and re-reads the count.
    // Runs when the public count goes from 0 to 1 again; children re-take their device reference.
    virtual void on_first_public_ref() noexcept {}
    // Runs when the public count reaches zero; children drop their device reference here.
    virtual void on_last_public_release() noexcept {}

private:
    RefCounts counts_;
};

template <typename Base, typename... Extra> Object<Base, Extra...>::Object() noexcept = default;

template <typename Base, typename... Extra> Object<Base, Extra...>::~Object() = default;

template <typename Base, typename... Extra>
HRESULT STDMETHODCALLTYPE Object<Base, Extra...>::QueryInterface(REFIID iid,
                                                                 void** object) noexcept {
    DM_IMPL("IUnknown::QueryInterface");
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = find_interface(iid);
    if (*object == nullptr) {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

template <typename Base, typename... Extra>
ULONG STDMETHODCALLTYPE Object<Base, Extra...>::AddRef() noexcept {
    DM_IMPL("IUnknown::AddRef");
    const ULONG count = counts_.add_ref();
    if (count == 1) {
        on_first_public_ref();
    }
    return count;
}

template <typename Base, typename... Extra>
ULONG STDMETHODCALLTYPE Object<Base, Extra...>::Release() noexcept {
    DM_IMPL("IUnknown::Release");
    const ULONG count = counts_.release();
    if (count == 0) {
        on_last_public_release();
        release_private();
    }
    return count;
}

template <typename Base, typename... Extra>
void Object<Base, Extra...>::add_private_ref() noexcept {
    counts_.add_private_ref();
}

template <typename Base, typename... Extra>
void Object<Base, Extra...>::release_private() noexcept {
    if (counts_.release_private()) {
        delete this;
    }
}

} // namespace directmetal::com
