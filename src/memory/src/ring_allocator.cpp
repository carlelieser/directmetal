#include "alignment.hpp"

#include <directmetal/memory/ring_allocator.hpp>

#include <cassert>
#include <deque>

namespace directmetal::memory {

// head_ and tail_ count bytes ever allocated and released, so head_ - tail_ is the space in use
// and head_ % capacity is the next offset. A region that does not fit before the end of the ring
// starts at 0 and also owns the skipped tail bytes until it retires.
class RingAllocator::State {
public:
    explicit State(std::uint64_t capacity) : capacity_(capacity) {}

    std::optional<std::uint64_t> allocate(std::uint64_t size, std::uint64_t alignment,
                                          std::uint64_t retire_value) {
        assert(regions_.empty() || regions_.back().retire_value <= retire_value);
        const std::uint64_t position = head_ % capacity_;
        auto offset = align_up(position, alignment);
        if (!offset || *offset > capacity_ || capacity_ - *offset < size) {
            offset = 0;
        }
        const std::uint64_t skipped =
            *offset >= position ? *offset - position : capacity_ - position;
        const std::uint64_t consumed = skipped + size;
        if (consumed > capacity_ - (head_ - tail_)) {
            return std::nullopt;
        }
        head_ += consumed;
        regions_.push_back({head_, retire_value});
        return offset;
    }

    void retire(std::uint64_t completed_value) {
        while (!regions_.empty() && regions_.front().retire_value <= completed_value) {
            tail_ = regions_.front().end;
            regions_.pop_front();
        }
        if (regions_.empty()) {
            head_ = 0;
            tail_ = 0;
        }
    }

    std::uint64_t capacity() const noexcept { return capacity_; }

private:
    struct Region {
        std::uint64_t end;
        std::uint64_t retire_value;
    };

    std::uint64_t capacity_;
    std::uint64_t head_ = 0;
    std::uint64_t tail_ = 0;
    std::deque<Region> regions_;
};

RingAllocator::RingAllocator(std::uint64_t capacity) : state_(std::make_unique<State>(capacity)) {}

RingAllocator::RingAllocator(RingAllocator&& other) noexcept = default;
RingAllocator& RingAllocator::operator=(RingAllocator&& other) noexcept = default;
RingAllocator::~RingAllocator() = default;

std::optional<std::uint64_t> RingAllocator::allocate(std::uint64_t size, std::uint64_t alignment,
                                                     std::uint64_t retire_value) {
    assert(is_power_of_two(alignment));
    const bool is_empty_or_oversize = size == 0 || size > state_->capacity();
    if (is_empty_or_oversize) {
        return std::nullopt;
    }
    return state_->allocate(size, alignment, retire_value);
}

void RingAllocator::retire(std::uint64_t completed_value) {
    state_->retire(completed_value);
}

std::uint64_t RingAllocator::capacity() const noexcept {
    return state_->capacity();
}

} // namespace directmetal::memory
