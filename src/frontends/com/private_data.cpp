#include <directmetal/com/private_data.hpp>

#include "unicode.hpp"

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

// A reference this store holds until it is committed to an entry or dropped.
using HeldInterface = std::unique_ptr<IUnknown, ReleaseInterface>;

HeldInterface hold(IUnknown* object) noexcept {
    if (object != nullptr) {
        object->AddRef();
    }
    return HeldInterface(object);
}

// The bytes of a debug-name entry, or null when it is absent or holds an interface.
template <typename Entry> const std::vector<std::byte>* name_bytes(const Entry* entry) noexcept {
    const bool has_bytes = entry != nullptr && entry->object == nullptr;
    return has_bytes ? &entry->bytes : nullptr;
}

// The debug name set last wins; an interface stored under a debug-name GUID is not a name.
std::string choose_name(const std::vector<std::byte>* wide, const std::vector<std::byte>* narrow,
                        bool is_wide_preferred) {
    const bool is_wide_chosen = is_wide_preferred || narrow == nullptr;
    if (wide != nullptr && is_wide_chosen) {
        return utf8_from_utf16(*wide);
    }
    return narrow != nullptr ? utf8_sanitized(*narrow) : std::string();
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

// An entry without bytes or an object is a removal. The stored interface is referenced before the
// lock is taken, and the replaced one released and the callback run after it is dropped: each may
// call back into this store.
void PrivateDataStore::store(Entry entry) {
    const bool is_name = is_debug_name(entry.guid);
    const bool is_removal = entry.bytes.empty() && entry.object == nullptr;
    HeldInterface added = hold(entry.object);
    HeldInterface replaced;
    NameChange name_change;
    {
        const std::scoped_lock lock(mutex_);
        // Everything that can throw comes first, so a failure leaves the store unchanged.
        entries_.reserve(entries_.size() + 1);
        if (is_name) {
            name_change.callback = debug_name_callback_;
        }
        if (name_change.callback) {
            name_change.name = debug_name_after(entry, is_removal);
        }
        if (is_name && !is_removal) {
            is_wide_name_preferred_ = IsEqualGUID(entry.guid, debug_object_name_wide);
        }
        replaced.reset(replace(entry.guid, is_removal ? nullptr : &entry));
        (void)added.release();
        if (is_name) {
            name_change.sequence = ++name_sequence_;
        }
    }
    replaced.reset();
    deliver(name_change);
}

void PrivateDataStore::deliver(const NameChange& change) {
    if (!change.callback) {
        return;
    }
    const std::scoped_lock lock(delivery_mutex_);
    if (change.sequence <= delivered_sequence_) {
        return;
    }
    delivered_sequence_ = change.sequence;
    change.callback(change.name);
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

std::string PrivateDataStore::debug_name_locked() const {
    return choose_name(name_bytes(find(debug_object_name_wide)),
                       name_bytes(find(debug_object_name)), is_wide_name_preferred_);
}

// The debug name once `change` is stored, without storing it.
std::string PrivateDataStore::debug_name_after(const Entry& change, bool is_removal) const {
    const bool is_wide_change = IsEqualGUID(change.guid, debug_object_name_wide);
    const Entry* changed = is_removal ? nullptr : &change;
    const Entry* wide = is_wide_change ? changed : find(debug_object_name_wide);
    const Entry* narrow = is_wide_change ? find(debug_object_name) : changed;
    const bool is_wide_preferred = is_removal ? is_wide_name_preferred_ : is_wide_change;
    return choose_name(name_bytes(wide), name_bytes(narrow), is_wide_preferred);
}

} // namespace directmetal::com
