#pragma once

#ifndef __OBJC__
#error "arena.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include "allocation.hpp"
#include "deferred_free.hpp"
#include "heap_pool.hpp"

#include <directmetal/backend/device.hpp>

#include <array>
#include <mutex>

namespace directmetal::backend::resources {

// Creates the Metal object for a resource: in heap at offset, or as its own allocation when heap
// is nil. Returns nil on failure.
using ResourceMaker = id<MTLResource> (^)(id<MTLHeap> heap, NSUInteger offset);

// A device's resource memory: placement heaps per storage mode, dedicated allocations at or above
// the threshold, and deferred frees. Thread-safe.
class Arena {
public:
    Arena(id<MTLDevice> device, const DeviceRequest& request);

    id<MTLDevice> device() const noexcept { return device_; }
    Result<PlacedResource> place(const MemoryRequirement& requirement, ResourceMaker make);
    // Returns the resource's memory once the tracked queues have completed their submitted work.
    void release(PlacedResource resource);
    void track(Queue& queue);
    void untrack(Queue& queue) noexcept;
    MemoryStats stats();

private:
    Result<PlacedResource> place_in_heap(const MemoryRequirement& requirement, ResourceMaker make);
    Result<PlacedResource> place_dedicated(const MemoryRequirement& requirement,
                                           ResourceMaker make);
    HeapPool& pool(StorageMode storage) noexcept;
    void free_now(const Allocation& allocation);
    void reclaim();

    id<MTLDevice> device_;
    std::uint64_t dedicated_threshold_;
    std::mutex mutex_;
    std::array<HeapPool, 2> pools_;
    DeferredFrees deferred_;
    std::uint64_t used_bytes_ = 0;
    std::uint64_t pending_free_bytes_ = 0;
    std::uint64_t dedicated_count_ = 0;
};

} // namespace directmetal::backend::resources
