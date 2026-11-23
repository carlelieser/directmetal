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

private:
    T* object_ = nullptr;
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

private:
    T* object_ = nullptr;
};

template <typename T> Ref<T> Ref<T>::adopt(T* object) noexcept {
    Ref ref;
    ref.object_ = object;
    return ref;
}

template <typename T> Ref<T> Ref<T>::retain(T* object) noexcept {
    if (object != nullptr) {
        object->AddRef();
    }
    return adopt(object);
}

template <typename T> Ref<T>::Ref(const Ref& other) noexcept : object_(other.object_) {
    if (object_ != nullptr) {
        object_->AddRef();
    }
}

template <typename T>
Ref<T>::Ref(Ref&& other) noexcept : object_(std::exchange(other.object_, nullptr)) {}

template <typename T> Ref<T>& Ref<T>::operator=(Ref other) noexcept {
    std::swap(object_, other.object_);
    return *this;
}

template <typename T> Ref<T>::~Ref() {
    if (object_ != nullptr) {
        object_->Release();
    }
}

template <typename T> T* Ref<T>::get() const noexcept {
    return object_;
}

template <typename T> T* Ref<T>::operator->() const noexcept {
    return object_;
}

template <typename T> Ref<T>::operator bool() const noexcept {
    return object_ != nullptr;
}

template <typename T> T* Ref<T>::detach() noexcept {
    return std::exchange(object_, nullptr);
}

template <typename T> PrivateRef<T>::PrivateRef(T* object) noexcept : object_(object) {
    if (object_ != nullptr) {
        object_->add_private_ref();
    }
}

template <typename T>
PrivateRef<T>::PrivateRef(const PrivateRef& other) noexcept : PrivateRef(other.object_) {}

template <typename T>
PrivateRef<T>::PrivateRef(PrivateRef&& other) noexcept
    : object_(std::exchange(other.object_, nullptr)) {}

template <typename T> PrivateRef<T>& PrivateRef<T>::operator=(PrivateRef other) noexcept {
    std::swap(object_, other.object_);
    return *this;
}

template <typename T> PrivateRef<T>::~PrivateRef() {
    if (object_ != nullptr) {
        object_->release_private();
    }
}

template <typename T> T* PrivateRef<T>::get() const noexcept {
    return object_;
}

template <typename T> T* PrivateRef<T>::operator->() const noexcept {
    return object_;
}

template <typename T> PrivateRef<T>::operator bool() const noexcept {
    return object_ != nullptr;
}

} // namespace directmetal::com
