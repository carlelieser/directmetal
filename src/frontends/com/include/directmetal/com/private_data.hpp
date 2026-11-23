#pragma once

#include <unknwn.h>

#include <functional>
#include <string>
#include <string_view>

namespace directmetal::com {

// Backing store for SetPrivateData, SetPrivateDataInterface and GetPrivateData. Thread-safe.
class PrivateDataStore {
public:
    PrivateDataStore();
    PrivateDataStore(const PrivateDataStore&) = delete;
    PrivateDataStore& operator=(const PrivateDataStore&) = delete;
    ~PrivateDataStore();

    // size 0 or null data removes the entry.
    HRESULT set(REFGUID guid, UINT size, const void* data);
    // AddRef on store, Release on overwrite, removal or destruction.
    HRESULT set_interface(REFGUID guid, IUnknown* object);
    // DXGI_ERROR_MORE_DATA when *size is too small, DXGI_ERROR_NOT_FOUND when absent.
    HRESULT get(REFGUID guid, UINT* size, void* data) const;
    // UTF-8 value of WKPDID_D3DDebugObjectName or WKPDID_D3DDebugObjectNameW.
    std::string debug_name() const;
    // Called with the new name whenever a debug name is set; forwards labels to the backend.
    void on_debug_name_changed(std::function<void(std::string_view)> callback);
};

} // namespace directmetal::com
