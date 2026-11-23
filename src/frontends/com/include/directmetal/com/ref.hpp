#pragma once

#include <unknwn.h>

#include <utility>

namespace directmetal::com {

// Owning public reference: AddRef on retain and copy, Release on destruction.
template <typename T> class Ref {
public:
    Ref() noexcept = default;
    static Ref adopt(T* object) noexcept;  // takes over an existing reference
    static Ref retain(T* object) noexcept; // adds a reference
    Ref(const Ref& other) noexcept;
    Ref(Ref&& other) noexcept;
    Ref& operator=(Ref other) noexcept;
    ~Ref();

    T* get() const noexcept;
    T* operator->() const noexcept;
    explicit operator bool() const noexcept;
    // Gives up ownership without releasing.
    T* detach() noexcept;
};

// Internal reference: keeps the object alive while its public count is zero, so an application
// releasing a view does not destroy a resource the device still uses.
template <typename T> class PrivateRef {
public:
    PrivateRef() noexcept = default;
    explicit PrivateRef(T* object) noexcept;
    PrivateRef(const PrivateRef& other) noexcept;
    PrivateRef(PrivateRef&& other) noexcept;
    PrivateRef& operator=(PrivateRef other) noexcept;
    ~PrivateRef();

    T* get() const noexcept;
    T* operator->() const noexcept;
    explicit operator bool() const noexcept;
};

} // namespace directmetal::com
