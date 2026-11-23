#include "arena.hpp"

#include <cassert>

namespace directmetal::backend::resources {

Arena::Arena(id<MTLDevice> device, const DeviceRequest& request)
    : device_(device), dedicated_threshold_(request.dedicated_threshold),
      pools_{HeapPool(device, StorageMode::shared, request.heap_block_size),
             HeapPool(device, StorageMode::gpu_private, request.heap_block_size)} {}

Result<PlacedResource> Arena::place(const MemoryRequirement& requirement, ResourceMaker make) {
    const std::lock_guard lock(mutex_);
    reclaim();
    const bool is_dedicated =
        requirement.storage == StorageMode::memoryless || requirement.size >= dedicated_threshold_;
    return is_dedicated ? place_dedicated(requirement, make) : place_in_heap(requirement, make);
}

Result<PlacedResource> Arena::place_in_heap(const MemoryRequirement& requirement,
                                            ResourceMaker make) {
    HeapPool& heaps = pool(requirement.storage);
    const auto range = heaps.reserve(requirement);
    if (!range) {
        return Result<PlacedResource>::failure(Status::out_of_memory);
    }
    id<MTLResource> resource = make(heaps.heap(range->heap_index), range->offset);
    if (resource == nil) {
        heaps.free(*range);
        return Result<PlacedResource>::failure(Status::out_of_memory);
    }
    used_bytes_ += requirement.size;
    return Result<PlacedResource>::success({resource, {requirement, false, *range}});
}

Result<PlacedResource> Arena::place_dedicated(const MemoryRequirement& requirement,
                                              ResourceMaker make) {
    id<MTLResource> resource = make(nil, 0);
    if (resource == nil) {
        return Result<PlacedResource>::failure(Status::out_of_memory);
    }
    used_bytes_ += requirement.size;
    ++dedicated_count_;
    return Result<PlacedResource>::success({resource, {requirement, true, {}}});
}

void Arena::release(PlacedResource resource) {
    const std::lock_guard lock(mutex_);
    const Allocation allocation = resource.allocation;
    used_bytes_ -= allocation.requirement.size;
    pending_free_bytes_ += allocation.requirement.size;
    if (auto ready = deferred_.defer(std::move(resource))) {
        ready->resource = nil;
        pending_free_bytes_ -= allocation.requirement.size;
        free_now(allocation);
    }
    reclaim();
}

void Arena::track(Queue& queue) {
    const std::lock_guard lock(mutex_);
    deferred_.track(queue);
}

void Arena::untrack(Queue& queue) noexcept {
    const std::lock_guard lock(mutex_);
    deferred_.untrack(queue);
}

MemoryStats Arena::stats() {
    const std::lock_guard lock(mutex_);
    reclaim();
    MemoryStats stats{};
    for (const HeapPool& heaps : pools_) {
        stats.heap_count += heaps.heap_count();
        stats.heap_bytes += heaps.heap_bytes();
    }
    stats.used_bytes = used_bytes_;
    stats.dedicated_count = dedicated_count_;
    stats.pending_free_bytes = pending_free_bytes_;
    return stats;
}

HeapPool& Arena::pool(StorageMode storage) noexcept {
    assert(storage != StorageMode::memoryless);
    return pools_[storage == StorageMode::shared ? 0 : 1];
}

void Arena::free_now(const Allocation& allocation) {
    if (allocation.is_dedicated) {
        --dedicated_count_;
        return;
    }
    pool(allocation.requirement.storage).free(allocation.range);
}

void Arena::reclaim() {
    for (const Allocation& allocation : deferred_.take_completed()) {
        pending_free_bytes_ -= allocation.requirement.size;
        free_now(allocation);
    }
}

} // namespace directmetal::backend::resources
