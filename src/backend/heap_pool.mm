#include "heap_pool.hpp"

#include "metal_options.hpp"

#include <algorithm>

namespace directmetal::backend::resources {

HeapPool::HeapPool(id<MTLDevice> device, StorageMode storage, std::uint64_t block_size)
    : device_(device), storage_(storage), block_size_(block_size) {}

std::optional<HeapRange> HeapPool::reserve(const MemoryRequirement& requirement) {
    for (std::size_t index = 0; index < blocks_.size(); ++index) {
        const auto offset = blocks_[index].ranges.allocate(requirement.size, requirement.alignment);
        if (offset) {
            return HeapRange{static_cast<std::uint32_t>(index), *offset};
        }
    }
    if (!add_block(requirement.size)) {
        return std::nullopt;
    }
    const auto offset = blocks_.back().ranges.allocate(requirement.size, requirement.alignment);
    if (!offset) {
        return std::nullopt;
    }
    return HeapRange{static_cast<std::uint32_t>(blocks_.size() - 1), *offset};
}

void HeapPool::free(HeapRange range) {
    blocks_[range.heap_index].ranges.free(range.offset);
}

id<MTLHeap> HeapPool::heap(std::uint32_t heap_index) const noexcept {
    return blocks_[heap_index].heap;
}

std::uint64_t HeapPool::heap_count() const noexcept {
    return blocks_.size();
}

std::uint64_t HeapPool::heap_bytes() const noexcept {
    return heap_bytes_;
}

// Tracked hazards serialize work per heap, so command recording needs no fences in Phase 1.
bool HeapPool::add_block(std::uint64_t minimum_size) {
    MTLHeapDescriptor* descriptor = [MTLHeapDescriptor new];
    descriptor.type = MTLHeapTypePlacement;
    descriptor.storageMode = metal_storage_mode(storage_);
    descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    descriptor.size = std::max(block_size_, minimum_size);
    id<MTLHeap> heap = [device_ newHeapWithDescriptor:descriptor];
    if (heap == nil) {
        return false;
    }
    blocks_.push_back({heap, memory::RangeAllocator(descriptor.size)});
    heap_bytes_ += descriptor.size;
    return true;
}

} // namespace directmetal::backend::resources
