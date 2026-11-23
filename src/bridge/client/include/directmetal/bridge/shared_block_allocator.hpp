#pragma once

#include <directmetal/bridge/commands.hpp>

#include <cstdint>
#include <memory>

namespace directmetal::bridge {

// Carves small shared blocks (StreamControl, timeline mirrors) out of regions obtained from
// directmetal_bridge_allocate_shared, so each block does not cost a 64 KiB VirtualAlloc granule
// under WoW64. Blocks are zeroed. Thread-safe.
class SharedBlockAllocator {
public:
    // Size of each region requested from directmetal_bridge_allocate_shared.
    static constexpr std::uint64_t region_size = 64 * 1024;

    SharedBlockAllocator();
    SharedBlockAllocator(const SharedBlockAllocator&) = delete;
    SharedBlockAllocator& operator=(const SharedBlockAllocator&) = delete;
    ~SharedBlockAllocator();

    // size is at most region_size; alignment is a power of two of at most 4096. Null when shared
    // memory is exhausted.
    void* allocate(std::uint64_t size, std::uint64_t alignment) noexcept;
    // block and size must match an earlier allocate. The unix side must no longer write the block
    // (destroy_timeline, close_stream or destroy_device has returned).
    void free(void* block, std::uint64_t size) noexcept;

    StreamControl* allocate_stream_control() noexcept;
    // An 8-byte-aligned timeline mirror.
    std::uint64_t* allocate_mirror() noexcept;

private:
    class State;
    std::unique_ptr<State> state_;
};

// The process-wide allocator used by DeviceClient and StagingRing.
SharedBlockAllocator& shared_blocks() noexcept;

} // namespace directmetal::bridge
