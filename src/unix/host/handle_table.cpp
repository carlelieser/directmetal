#include "handle_table.hpp"

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
        // Reserved first, so remove() never allocates.
        free_.reserve(slots_.size() + 1);
        slots_.emplace_back();
        free_.push_back(static_cast<std::uint32_t>(slots_.size() - 1));
    }
    const std::uint32_t index = free_.back();
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
    if (++slot.generation <= generation_limit) {
        free_.push_back(index);
    }
    return entry;
}

std::vector<TableEntry> HandleTable::owned_by(bridge::Handle owner) const {
    const std::lock_guard lock(mutex_);
    std::vector<TableEntry> entries;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        const Slot& slot = slots_[index];
        if (slot.object != nullptr && slot.owner.value == owner.value) {
            const auto handle =
                make_handle(slot.kind, slot.generation, static_cast<std::uint32_t>(index));
            entries.push_back({handle, slot.kind, slot.owner, slot.object});
        }
    }
    return entries;
}

} // namespace directmetal::host
