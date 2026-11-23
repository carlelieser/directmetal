#include "hresult.hpp"

#include <directmetal/runtime/log.hpp>

#include <algorithm>
#include <array>

namespace directmetal::d3d11 {
namespace {

struct StatusMapping {
    backend::Status status;
    HRESULT result;
};

constexpr std::array status_mappings = {
    StatusMapping{backend::Status::success, S_OK},
    StatusMapping{backend::Status::out_of_memory, E_OUTOFMEMORY},
    StatusMapping{backend::Status::invalid_argument, E_INVALIDARG},
    StatusMapping{backend::Status::device_lost, DXGI_ERROR_DEVICE_REMOVED},
    StatusMapping{backend::Status::timeout, DXGI_ERROR_WAS_STILL_DRAWING},
};

struct PrivateDataMapping {
    com::PrivateDataResult status;
    HRESULT result;
};

constexpr std::array private_data_mappings = {
    PrivateDataMapping{com::PrivateDataResult::ok, S_OK},
    PrivateDataMapping{com::PrivateDataResult::more_data, DXGI_ERROR_MORE_DATA},
    PrivateDataMapping{com::PrivateDataResult::not_found, DXGI_ERROR_NOT_FOUND},
    PrivateDataMapping{com::PrivateDataResult::invalid_argument, E_INVALIDARG},
};

template <typename Mappings, typename Key> HRESULT lookup(const Mappings& mappings, Key key) {
    const auto mapping = std::find_if(mappings.begin(), mappings.end(),
                                      [key](const auto& item) { return item.status == key; });
    return mapping == mappings.end() ? E_FAIL : mapping->result;
}

} // namespace

HRESULT to_hresult(backend::Status status, HRESULT unsupported) noexcept {
    return status == backend::Status::unsupported ? unsupported : lookup(status_mappings, status);
}

HRESULT to_hresult(com::PrivateDataResult result) noexcept {
    return lookup(private_data_mappings, result);
}

HRESULT not_implemented(std::string_view operation, std::string_view feature) noexcept {
    log::print(log::Component::d3d11, log::Level::warn, "{}: {} is not implemented", operation,
               feature);
    return E_NOTIMPL;
}

void report_ignored_call(std::string_view operation, std::string_view reason) noexcept {
    log::print(log::Component::d3d11, log::Level::warn, "{}: call ignored: {}", operation, reason);
}

} // namespace directmetal::d3d11
