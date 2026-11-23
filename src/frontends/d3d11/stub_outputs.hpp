#pragma once

#include <d3d11.h>

#include <cstring>

// Clearing of a stub's output parameters, so callers never read uninitialized values. Each returns
// E_NOTIMPL, so a stub can return the result; void stubs discard it.
namespace directmetal::d3d11::stub {

template <typename... Outputs> HRESULT clear_outputs(Outputs*... outputs) noexcept {
    ((outputs != nullptr ? static_cast<void>(*outputs = Outputs{}) : static_cast<void>(0)), ...);
    return E_NOTIMPL;
}

template <typename Element> HRESULT clear_array(UINT count, Element* array) noexcept {
    for (UINT index = 0; array != nullptr && index < count; ++index) {
        array[index] = Element{};
    }
    return E_NOTIMPL;
}

inline HRESULT clear_bytes(void* data, UINT size) noexcept {
    if (data != nullptr) {
        std::memset(data, 0, size);
    }
    return E_NOTIMPL;
}

// Combines several clears in one stub return value.
template <typename... Results> HRESULT all(Results...) noexcept {
    return E_NOTIMPL;
}

} // namespace directmetal::d3d11::stub
