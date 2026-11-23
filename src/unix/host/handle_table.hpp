#pragma once

#include <directmetal/bridge/protocol.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Handles: kind in bits 56-63, generation in bits 32-55, slot index in bits 0-31. A slot's
// generation starts at 1 and increments when its object is removed; a slot whose generation is
// exhausted is never used again, so no handle value repeats within the table's lifetime.
namespace directmetal::host {

enum class ObjectKind : std::uint8_t {
    none,
    device,
    buffer,
    imported_buffer,
    texture,
    timeline,
    progress_timeline,
    stream,
};

ObjectKind kind_of(bridge::Handle handle) noexcept;

struct TableEntry {
    bridge::Handle handle;
    ObjectKind kind;
    // The device that owns the object; null for devices.
    bridge::Handle owner;
    std::shared_ptr<void> object;
};

// Thread-safe.
class HandleTable {
public:
    // A null handle once every slot index is in use.
    bridge::Handle insert(ObjectKind kind, bridge::Handle owner, std::shared_ptr<void> object);
    // The live entry handle names, or an entry with a null object.
    TableEntry find(bridge::Handle handle) const;
    // Removes the entry handle names, which must be of kind; null object if there is none.
    TableEntry remove(bridge::Handle handle, ObjectKind kind);
    std::vector<TableEntry> owned_by(bridge::Handle owner) const;

    template <typename T> std::shared_ptr<T> find_as(bridge::Handle handle, ObjectKind kind) const {
        const TableEntry entry = find(handle);
        return entry.kind == kind ? std::static_pointer_cast<T>(entry.object) : nullptr;
    }

private:
    struct Slot {
        std::uint32_t generation = 1;
        ObjectKind kind = ObjectKind::none;
        bridge::Handle owner{};
        std::shared_ptr<void> object;
    };

    const Slot* live_slot(bridge::Handle handle) const noexcept;
    void forget_owned(bridge::Handle owner, std::uint32_t index) noexcept;

    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_;
    // Slot indexes of the live objects of each owner, keyed by the owner's handle value.
    std::unordered_map<std::uint64_t, std::unordered_set<std::uint32_t>> owned_;
};

} // namespace directmetal::host
