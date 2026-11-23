#include "handle_table.hpp"

#include <algorithm>
#include <utility>

namespace directmetal::host {
namespace {

constexpr unsigned kind_shift = 56;
constexpr unsigned generation_shift = 32;
constexpr std::uint32_t generation_limit = (1u << 24) - 1;
constexpr std::uint64_t index_mask = 0xFFFFFFFF;
constexpr std::size_t max_slots = index_mask + 1;

bridge::Handle make_handle(ObjectKind kind, std::uint32_t generation, std::uint32_t index) {
    const auto kind_bits = static_cast<std::uint64_t>(kind) << kind_shift;
    const auto generation_bits = static_cast<std::uint64_t>(generation) << generation_shift;
    return {kind_bits | generation_bits | index};
}

std::uint32_t generation_of(bridge::Handle handle) noexcept {
    return static_cast<std::uint32_t>(handle.value >> generation_shift) & generation_limit;
}

} // namespace

ObjectKind kind_of(bridge::Handle handle) noexcept {
    return static_cast<ObjectKind>(handle.value >> kind_shift);
}

bridge::Handle HandleTable::insert(ObjectKind kind, bridge::Handle owner,
                                   std::shared_ptr<void> object) {
    const std::lock_guard lock(mutex_);
    if (free_.empty() && slots_.size() == max_slots) {
        return {};
    }
    if (free_.empty()) {
        // Reserved first, so remove() never allocates; grown geometrically like slots_.
        if (free_.capacity() <= slots_.size()) {
            free_.reserve(std::max<std::size_t>(16, 2 * (slots_.size() + 1)));
        }
        slots_.emplace_back();
        free_.push_back(static_cast<std::uint32_t>(slots_.size() - 1));
    }
    const std::uint32_t index = free_.back();
    // Indexed before the slot changes, so a failed insertion leaves the table as it was.
    owned_[owner.value].insert(index);
    free_.pop_back();
    Slot& slot = slots_[index];
    slot.kind = kind;
    slot.owner = owner;
    slot.object = std::move(object);
    return make_handle(kind, slot.generation, index);
}

const HandleTable::Slot* HandleTable::live_slot(bridge::Handle handle) const noexcept {
    const std::uint64_t index = handle.value & index_mask;
    if (index >= slots_.size()) {
        return nullptr;
    }
    const Slot& slot = slots_[index];
    const bool is_current = slot.generation == generation_of(handle) && slot.object != nullptr;
    return is_current && slot.kind == kind_of(handle) ? &slot : nullptr;
}

TableEntry HandleTable::find(bridge::Handle handle) const {
    const std::lock_guard lock(mutex_);
    const Slot* slot = live_slot(handle);
    if (slot == nullptr) {
        return {handle, ObjectKind::none, {}, nullptr};
    }
    return {handle, slot->kind, slot->owner, slot->object};
}

TableEntry HandleTable::remove(bridge::Handle handle, ObjectKind kind) {
    const std::lock_guard lock(mutex_);
    const Slot* found = live_slot(handle);
    if (found == nullptr || found->kind != kind) {
        return {handle, ObjectKind::none, {}, nullptr};
    }
    const auto index = static_cast<std::uint32_t>(handle.value & index_mask);
    Slot& slot = slots_[index];
    TableEntry entry{handle, slot.kind, slot.owner, std::move(slot.object)};
    slot.object = nullptr;
    slot.kind = ObjectKind::none;
    forget_owned(entry.owner, index);
    if (++slot.generation <= generation_limit) {
        free_.push_back(index);
    }
    return entry;
}

std::vector<TableEntry> HandleTable::owned_by(bridge::Handle owner) const {
    const std::lock_guard lock(mutex_);
    std::vector<TableEntry> entries;
    const auto owned = owned_.find(owner.value);
    if (owned == owned_.end()) {
        return entries;
    }
    entries.reserve(owned->second.size());
    for (const std::uint32_t index : owned->second) {
        const Slot& slot = slots_[index];
        const auto handle = make_handle(slot.kind, slot.generation, index);
        entries.push_back({handle, slot.kind, slot.owner, slot.object});
    }
    return entries;
}

void HandleTable::forget_owned(bridge::Handle owner, std::uint32_t index) noexcept {
    const auto owned = owned_.find(owner.value);
    owned->second.erase(index);
    if (owned->second.empty()) {
        owned_.erase(owned);
    }
}

} // namespace directmetal::host
