#include <directmetal/com/private_data.hpp>

#include "utf16.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <span>
#include <utility>

namespace directmetal::com {
namespace {

// https://learn.microsoft.com/windows/win32/direct3d11/using-the-debug-layer-to-test-apps
constexpr GUID debug_object_name = {
    0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};
constexpr GUID debug_object_name_wide = {
    0x4cca5fd8, 0x921f, 0x42c8, {0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41}};

bool is_debug_name(REFGUID guid) noexcept {
    return IsEqualGUID(guid, debug_object_name) || IsEqualGUID(guid, debug_object_name_wide);
}

struct ReleaseInterface {
    void operator()(IUnknown* object) const noexcept { object->Release(); }
};

using ReplacedInterface = std::unique_ptr<IUnknown, ReleaseInterface>;

std::string narrow_name(std::span<const std::byte> bytes) {
    const auto* begin = reinterpret_cast<const char*>(bytes.data());
    const auto* end = std::find(begin, begin + bytes.size(), '\0');
    return std::string(begin, end);
}

} // namespace

PrivateDataStore::PrivateDataStore() = default;

PrivateDataStore::~PrivateDataStore() {
    for (const Entry& entry : entries_) {
        if (entry.object != nullptr) {
            entry.object->Release();
        }
    }
}

PrivateDataResult PrivateDataStore::set(REFGUID guid, UINT size, const void* data) {
    if (size == 0) {
        store(Entry{guid, {}, nullptr});
        return PrivateDataResult::ok;
    }
    if (data == nullptr) {
        return PrivateDataResult::invalid_argument;
    }
    const auto* begin = static_cast<const std::byte*>(data);
    store(Entry{guid, std::vector<std::byte>(begin, begin + size), nullptr});
    return PrivateDataResult::ok;
}

PrivateDataResult PrivateDataStore::set_interface(REFGUID guid, IUnknown* object) {
    store(Entry{guid, {}, object});
    return PrivateDataResult::ok;
}

PrivateDataResult PrivateDataStore::get(REFGUID guid, UINT* size, void* data) const {
    if (size == nullptr) {
        return PrivateDataResult::invalid_argument;
    }
    const std::scoped_lock lock(mutex_);
    const Entry* entry = find(guid);
    if (entry == nullptr) {
        *size = 0;
        return PrivateDataResult::not_found;
    }
    const bool has_object = entry->object != nullptr;
    const UINT required = has_object ? UINT{sizeof(IUnknown*)} : UINT(entry->bytes.size());
    const bool is_too_small = data != nullptr && *size < required;
    *size = required;
    if (data == nullptr) {
        return PrivateDataResult::ok;
    }
    if (is_too_small) {
        return PrivateDataResult::more_data;
    }
    if (has_object) {
        entry->object->AddRef();
        std::memcpy(data, &entry->object, sizeof(IUnknown*));
    } else {
        std::memcpy(data, entry->bytes.data(), entry->bytes.size());
    }
    return PrivateDataResult::ok;
}

std::string PrivateDataStore::debug_name() const {
    const std::scoped_lock lock(mutex_);
    return debug_name_locked();
}

void PrivateDataStore::on_debug_name_changed(std::function<void(std::string_view)> callback) {
    const std::scoped_lock lock(mutex_);
    debug_name_callback_ = std::move(callback);
}

// An entry without bytes or an object is a removal. The replaced interface is released and the
// callback runs after the lock is dropped: either may call back into this store.
void PrivateDataStore::store(Entry entry) {
    const bool is_name = is_debug_name(entry.guid);
    const bool is_removal = entry.bytes.empty() && entry.object == nullptr;
    const bool is_wide_name = IsEqualGUID(entry.guid, debug_object_name_wide);
    IUnknown* const object = entry.object;
    ReplacedInterface replaced;
    DebugNameCallback callback;
    std::string name;
    {
        const std::scoped_lock lock(mutex_);
        if (is_name) {
            callback = debug_name_callback_;
        }
        replaced.reset(replace(entry.guid, is_removal ? nullptr : &entry));
        if (object != nullptr) {
            object->AddRef();
        }
        if (is_name && !is_removal) {
            is_wide_name_preferred_ = is_wide_name;
        }
        if (callback) {
            name = debug_name_locked();
        }
    }
    replaced.reset();
    if (callback) {
        callback(name);
    }
}

IUnknown* PrivateDataStore::replace(REFGUID guid, Entry* replacement) {
    auto existing = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const Entry& entry) { return IsEqualGUID(entry.guid, guid); });
    if (existing == entries_.end()) {
        if (replacement != nullptr) {
            entries_.push_back(std::move(*replacement));
        }
        return nullptr;
    }
    IUnknown* replaced = existing->object;
    if (replacement != nullptr) {
        *existing = std::move(*replacement);
    } else {
        entries_.erase(existing);
    }
    return replaced;
}

const PrivateDataStore::Entry* PrivateDataStore::find(REFGUID guid) const noexcept {
    auto existing = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const Entry& entry) { return IsEqualGUID(entry.guid, guid); });
    return existing == entries_.end() ? nullptr : &*existing;
}

// The debug name set last wins; an interface stored under a debug-name GUID is not a name.
std::string PrivateDataStore::debug_name_locked() const {
    const Entry* wide = find(debug_object_name_wide);
    const Entry* narrow = find(debug_object_name);
    const bool has_wide = wide != nullptr && wide->object == nullptr;
    const bool has_narrow = narrow != nullptr && narrow->object == nullptr;
    const bool is_wide_chosen = is_wide_name_preferred_ || !has_narrow;
    if (has_wide && is_wide_chosen) {
        return utf8_from_utf16(wide->bytes);
    }
    return has_narrow ? narrow_name(narrow->bytes) : std::string();
}

} // namespace directmetal::com
