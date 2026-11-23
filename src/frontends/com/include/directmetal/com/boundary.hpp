#pragma once

#include <directmetal/runtime/log.hpp>

#include <unknwn.h>

#include <new>
#include <string_view>
#include <utility>

namespace directmetal::com {

// Runs a COM method body at the API boundary. Allocation failure becomes E_OUTOFMEMORY; any other
// exception is a bug and terminates through noexcept.
template <typename Body> HRESULT guard(std::string_view operation, Body&& body) noexcept {
    try {
        return std::forward<Body>(body)();
    } catch (const std::bad_alloc&) {
        log::print(log::Component::com, log::Level::error, "{}: out of memory", operation);
        return E_OUTOFMEMORY;
    }
}

} // namespace directmetal::com
