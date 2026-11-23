#include "alignment.hpp"

#include <directmetal/memory/range_allocator.hpp>

#include <cassert>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>

namespace directmetal::memory {

// Free ranges are kept by offset, for first fit and coalescing, and by size, for largest_free.
class RangeAllocator::State {
public:
    explicit State(std::uint64_t capacity) : capacity_(capacity) {
        if (capacity > 0) {
            insert_free(0, capacity);
        }
    }

    std::optional<std::uint64_t> allocate(std::uint64_t size, std::uint64_t alignment) {
        for (auto range = free_.begin(); range != free_.end(); ++range) {
            const auto placed = place(range->first, range->second, size, alignment);
            if (placed) {
                take(range, *placed, size);
                return placed;
            }
        }
        return std::nullopt;
    }

    void free(std::uint64_t offset) {
        const auto allocation = allocated_.find(offset);
        assert(allocation != allocated_.end());
        std::uint64_t begin = offset;
        std::uint64_t end = offset + allocation->second;
        allocated_.erase(allocation);
        auto next = free_.lower_bound(begin);
        if (next != free_.begin()) {
            const auto previous = std::prev(next);
            if (previous->first + previous->second == begin) {
                begin = previous->first;
                erase_free(previous);
            }
        }
        if (next != free_.end() && next->first == end) {
            end += next->second;
            erase_free(next);
        }
        insert_free(begin, end - begin);
    }

    std::uint64_t largest_free() const noexcept { return sizes_.empty() ? 0 : *sizes_.rbegin(); }

    std::uint64_t capacity() const noexcept { return capacity_; }

private:
    using FreeRanges = std::map<std::uint64_t, std::uint64_t>;

    static std::optional<std::uint64_t> place(std::uint64_t offset, std::uint64_t length,
                                              std::uint64_t size, std::uint64_t alignment) {
        const auto aligned = align_up(offset, alignment);
        if (!aligned || *aligned - offset > length) {
            return std::nullopt;
        }
        if (length - (*aligned - offset) < size) {
            return std::nullopt;
        }
        return aligned;
    }

    // Splits the free range at it into the alignment padding, the allocation and the remainder.
    void take(FreeRanges::iterator range, std::uint64_t offset, std::uint64_t size) {
        const std::uint64_t begin = range->first;
        const std::uint64_t end = begin + range->second;
        erase_free(range);
        if (offset > begin) {
            insert_free(begin, offset - begin);
        }
        if (offset + size < end) {
            insert_free(offset + size, end - (offset + size));
        }
        allocated_.emplace(offset, size);
    }

    void insert_free(std::uint64_t offset, std::uint64_t size) {
        free_.emplace(offset, size);
        sizes_.insert(size);
    }

    void erase_free(FreeRanges::iterator range) {
        sizes_.erase(sizes_.find(range->second));
        free_.erase(range);
    }

    std::uint64_t capacity_;
    FreeRanges free_;
    std::multiset<std::uint64_t> sizes_;
    std::unordered_map<std::uint64_t, std::uint64_t> allocated_;
};

RangeAllocator::RangeAllocator(std::uint64_t capacity)
    : state_(std::make_unique<State>(capacity)) {}

RangeAllocator::RangeAllocator(RangeAllocator&& other) noexcept = default;
RangeAllocator& RangeAllocator::operator=(RangeAllocator&& other) noexcept = default;
RangeAllocator::~RangeAllocator() = default;

std::optional<std::uint64_t> RangeAllocator::allocate(std::uint64_t size, std::uint64_t alignment) {
    assert(is_power_of_two(alignment));
    if (size == 0) {
        return std::nullopt;
    }
    return state_->allocate(size, alignment);
}

void RangeAllocator::free(std::uint64_t offset) {
    state_->free(offset);
}

std::uint64_t RangeAllocator::largest_free() const noexcept {
    return state_->largest_free();
}

std::uint64_t RangeAllocator::capacity() const noexcept {
    return state_->capacity();
}

} // namespace directmetal::memory
