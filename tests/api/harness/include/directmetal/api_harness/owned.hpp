#pragma once

namespace directmetal::api_harness {

// Owns one COM reference and releases it on destruction.
template <typename Interface> class Owned {
public:
    Owned() = default;
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    ~Owned() {
        if (pointer_ != nullptr) {
            pointer_->Release();
        }
    }
    Interface** put() { return &pointer_; }
    Interface* get() const { return pointer_; }
    Interface* operator->() const { return pointer_; }
    auto release() {
        Interface* pointer = pointer_;
        pointer_ = nullptr;
        return pointer->Release();
    }

private:
    Interface* pointer_ = nullptr;
};

} // namespace directmetal::api_harness
