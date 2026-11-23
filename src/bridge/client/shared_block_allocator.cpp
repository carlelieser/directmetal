#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/shared_block_allocator.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace directmetal::bridge {
namespace {

// Every block is a multiple of this, so free ranges stay 8-byte aligned.
constexpr std::uint64_t block_granule = 8;
constexpr std::uint64_t max_alignment = 4096;

constexpr std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

bool is_valid_request(std::uint64_t size, std::uint64_t alignment) noexcept {
    const bool is_power_of_two = alignment != 0 && (alignment & (alignment - 1)) == 0;
    const bool has_valid_size = size != 0 && size <= SharedBlockAllocator::region_size;
    return is_power_of_two && alignment <= max_alignment && has_valid_size;
}

struct Range {
    std::uint64_t offset;
    std::uint64_t size;
};

// First-fit over one region's free ranges, kept sorted by offset and coalesced. A region has at
// most one more free range than live blocks, so capacity reserved on allocation lets free run
// without allocating.
struct Region {
    std::byte* memory;
    std::vector<Range> free_ranges;
    std::size_t live_blocks = 0;

    bool contains(const std::byte* block) const noexcept {
        return block >= memory && block < memory + SharedBlockAllocator::region_size;
    }

    std::byte* allocate(std::uint64_t size, std::uint64_t alignment) {
        free_ranges.reserve(live_blocks + 2);
        for (auto range = free_ranges.begin(); range != free_ranges.end(); ++range) {
            const std::uint64_t start = align_up(range->offset, alignment);
            const std::uint64_t end = range->offset + range->size;
            if (start + size > end) {
                continue;
            }
            const Range before{range->offset, start - range->offset};
            const Range after{start + size, end - start - size};
            range = free_ranges.erase(range);
            if (after.size != 0) {
                range = free_ranges.insert(range, after);
            }
            if (before.size != 0) {
                free_ranges.insert(range, before);
            }
            ++live_blocks;
            return memory + start;
        }
        return nullptr;
    }

    void free(std::uint64_t offset, std::uint64_t size) noexcept {
        --live_blocks;
        auto next = std::find_if(free_ranges.begin(), free_ranges.end(),
                                 [offset](const Range& range) { return range.offset > offset; });
        auto inserted = free_ranges.insert(next, Range{offset, size});
        if (auto following = inserted + 1; following != free_ranges.end() &&
                                           inserted->offset + inserted->size == following->offset) {
            inserted->size += following->size;
            free_ranges.erase(following);
        }
        if (inserted != free_ranges.begin()) {
            auto previous = inserted - 1;
            if (previous->offset + previous->size == inserted->offset) {
                previous->size += inserted->size;
                free_ranges.erase(inserted);
            }
        }
    }
};

} // namespace

class SharedBlockAllocator::State {
public:
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    ~State() {
        for (const Region& region : regions_) {
            directmetal_bridge_free_shared(region.memory, region_size);
        }
    }

    std::byte* allocate(std::uint64_t size, std::uint64_t alignment) {
        const std::lock_guard lock(mutex_);
        for (Region& region : regions_) {
            if (std::byte* block = region.allocate(size, alignment); block != nullptr) {
                return block;
            }
        }
        return allocate_in_new_region(size, alignment);
    }

    void free(std::byte* block, std::uint64_t size) noexcept {
        const std::lock_guard lock(mutex_);
        const auto region =
            std::find_if(regions_.begin(), regions_.end(),
                         [block](const Region& item) { return item.contains(block); });
        assert(region != regions_.end());
        if (region == regions_.end()) {
            return;
        }
        region->free(static_cast<std::uint64_t>(block - region->memory), size);
    }

private:
    std::byte* allocate_in_new_region(std::uint64_t size, std::uint64_t alignment) {
        // Reserved first, so a throwing push_back cannot leak the shared memory.
        regions_.reserve(regions_.size() + 1);
        Region region{nullptr, {{0, region_size}}};
        region.memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(region_size));
        if (region.memory == nullptr) {
            return nullptr;
        }
        regions_.push_back(std::move(region));
        return regions_.back().allocate(size, alignment);
    }

    std::mutex mutex_;
    // Kept for the allocator's lifetime: blocks are small and reused.
    std::vector<Region> regions_;
};

SharedBlockAllocator::SharedBlockAllocator() : state_(std::make_unique<State>()) {}

SharedBlockAllocator::~SharedBlockAllocator() = default;

void* SharedBlockAllocator::allocate(std::uint64_t size, std::uint64_t alignment) noexcept {
    if (!is_valid_request(size, alignment)) {
        return nullptr;
    }
    const std::uint64_t granular_size = align_up(size, block_granule);
    try {
        std::byte* block = state_->allocate(granular_size, std::max(alignment, block_granule));
        if (block != nullptr) {
            std::memset(block, 0, granular_size);
        }
        return block;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void SharedBlockAllocator::free(void* block, std::uint64_t size) noexcept {
    if (block != nullptr) {
        state_->free(static_cast<std::byte*>(block), align_up(size, block_granule));
    }
}

StreamControl* SharedBlockAllocator::allocate_stream_control() noexcept {
    void* block = allocate(sizeof(StreamControl), alignof(StreamControl));
    return block == nullptr ? nullptr : new (block) StreamControl{};
}

std::uint64_t* SharedBlockAllocator::allocate_mirror() noexcept {
    void* block = allocate(sizeof(std::uint64_t), alignof(std::uint64_t));
    return block == nullptr ? nullptr : new (block) std::uint64_t{0};
}

// Never destroyed: the unix side may still write blocks of leaked objects while the process exits.
SharedBlockAllocator& shared_blocks() noexcept {
    static auto* const instance = new SharedBlockAllocator();
    return *instance;
}

} // namespace directmetal::bridge
