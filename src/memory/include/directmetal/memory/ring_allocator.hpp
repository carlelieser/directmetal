#pragma once

#include <cstdint>
#include <memory>
#include <optional>

namespace directmetal::memory {

// Ring over [0, capacity). Each allocation is tagged with the timeline value whose completion
// frees it; regions are released in allocation order. Not thread-safe; callers lock.
class RingAllocator {
public:
    explicit RingAllocator(std::uint64_t capacity);
    RingAllocator(RingAllocator&& other) noexcept;
    RingAllocator& operator=(RingAllocator&& other) noexcept;
    ~RingAllocator();

    // Returns the offset of a contiguous region, or nullopt when the ring is full. alignment is a
    // power of two; retire_value must not decrease between calls.
    std::optional<std::uint64_t> allocate(std::uint64_t size, std::uint64_t alignment,
                                          std::uint64_t retire_value);
    // Frees every region tagged with a value at or below completed_value.
    void retire(std::uint64_t completed_value);
    std::uint64_t capacity() const noexcept;

private:
    class State;
    std::unique_ptr<State> state_;
};

} // namespace directmetal::memory
