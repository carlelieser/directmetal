#pragma once

#ifndef __OBJC__
#error "allocation.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include <directmetal/backend/types.hpp>

#include <cstdint>

namespace directmetal::backend::resources {

// The memory a resource needs, from MTLDevice's heap size-and-align queries.
struct MemoryRequirement {
    std::uint64_t size;
    std::uint64_t alignment;
    StorageMode storage;
};

struct HeapRange {
    std::uint32_t heap_index;
    std::uint64_t offset;
};

struct Allocation {
    MemoryRequirement requirement;
    bool is_dedicated;
    HeapRange range;
};

// A resource and the memory behind it. Holding the resource keeps the Metal object alive until
// its memory is returned.
struct PlacedResource {
    id<MTLResource> resource;
    Allocation allocation;
};

} // namespace directmetal::backend::resources
