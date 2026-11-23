#pragma once

#include <unknwn.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace directmetal::com {

// Front ends map these to the API's codes (DXGI_ERROR_MORE_DATA, DXGI_ERROR_NOT_FOUND,
// D3DERR_MOREDATA, D3DERR_NOTFOUND, E_INVALIDARG).
enum class PrivateDataResult : std::uint8_t { ok, more_data, not_found, invalid_argument };

// Backing store for SetPrivateData, SetPrivateDataInterface and GetPrivateData. Thread-safe.
class PrivateDataStore {
public:
    PrivateDataStore();
    PrivateDataStore(const PrivateDataStore&) = delete;
    PrivateDataStore& operator=(const PrivateDataStore&) = delete;
    ~PrivateDataStore();

    // size 0 removes the entry; a non-zero size with null data is invalid_argument.
    PrivateDataResult set(REFGUID guid, UINT size, const void* data);
    // AddRef on store, Release on overwrite, removal or destruction. Null removes the entry.
    PrivateDataResult set_interface(REFGUID guid, IUnknown* object);
    // Null data queries the size. more_data when *size is too small (and *size is set to the
    // required size); not_found sets *size to 0.
    PrivateDataResult get(REFGUID guid, UINT* size, void* data) const;
    // UTF-8 value of WKPDID_D3DDebugObjectName or WKPDID_D3DDebugObjectNameW.
    std::string debug_name() const;
    // Called with the new name whenever a debug name is set, outside the store's lock; forwards
    // labels to the backend.
    void on_debug_name_changed(std::function<void(std::string_view)> callback);
};

} // namespace directmetal::com
