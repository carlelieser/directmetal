#pragma once

#ifndef __OBJC__
#error "heap_pool.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include "allocation.hpp"

#include <directmetal/memory/range_allocator.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace directmetal::backend::resources {

// Placement heaps of one storage mode, each with a RangeAllocator over its bytes. A request that
// fits no heap adds one of at least block_size bytes. Not thread-safe; the arena locks.
class HeapPool {
public:
    HeapPool(id<MTLDevice> device, StorageMode storage, std::uint64_t block_size);

    std::optional<HeapRange> reserve(const MemoryRequirement& requirement);
    void free(HeapRange range);
    id<MTLHeap> heap(std::uint32_t heap_index) const noexcept;
    std::uint64_t heap_count() const noexcept;
    std::uint64_t heap_bytes() const noexcept;

private:
    struct Block {
        id<MTLHeap> heap;
        memory::RangeAllocator ranges;
    };

    bool add_block(std::uint64_t minimum_size);

    id<MTLDevice> device_;
    StorageMode storage_;
    std::uint64_t block_size_;
    std::vector<Block> blocks_;
    std::uint64_t heap_bytes_ = 0;
};

} // namespace directmetal::backend::resources
