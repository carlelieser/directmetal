#pragma once

#include <cstdint>
#include <memory>
#include <optional>

namespace directmetal::memory {

// First-fit allocator over [0, capacity) with coalescing on free. Not thread-safe; callers lock.
class RangeAllocator {
public:
    explicit RangeAllocator(std::uint64_t capacity);
    RangeAllocator(RangeAllocator&& other) noexcept;
    RangeAllocator& operator=(RangeAllocator&& other) noexcept;
    ~RangeAllocator();

    // Returns the offset of a free range, or nullopt when no range fits. alignment is a power of
    // two.
    std::optional<std::uint64_t> allocate(std::uint64_t size, std::uint64_t alignment);
    // offset must come from allocate and not have been freed since.
    void free(std::uint64_t offset);
    std::uint64_t largest_free() const noexcept;
    std::uint64_t capacity() const noexcept;

private:
    class State;
    std::unique_ptr<State> state_;
};

} // namespace directmetal::memory
