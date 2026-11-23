#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/memory/ring_allocator.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace directmetal::bridge {

// Upload and readback staging: a ring over shared memory imported once as a unix-side buffer.
// Regions are retired against a timeline's completed value. Used by one thread at a time.
//
// The ring is destroyed before its device, after every command naming its buffer has been
// submitted and flushed: destruction calls release_memory, then frees the memory. When
// release_memory fails the unix side may still use the memory, so it is leaked instead.
class StagingRing {
public:
    struct Allocation {
        std::byte* cpu_address;
        Handle buffer;
        std::uint64_t offset;
    };

    // capacity is rounded up to shared_memory_alignment. Fails with unsupported when the import is
    // not zero-copy, since CPU and GPU must then see the same bytes.
    static backend::Result<StagingRing> create(DeviceClient& device, std::uint64_t capacity);

    StagingRing(StagingRing&& other) noexcept;
    StagingRing& operator=(StagingRing&&) = delete;
    StagingRing(const StagingRing&) = delete;
    StagingRing& operator=(const StagingRing&) = delete;
    ~StagingRing();

    // Returns nullopt when the ring is full; retire_value is the timeline value whose completion
    // frees the region. alignment is a power of two; retire_value must not decrease between calls.
    std::optional<Allocation> allocate(std::uint64_t size, std::uint64_t alignment,
                                       std::uint64_t retire_value);
    void retire(std::uint64_t completed_value);

private:
    StagingRing(DeviceClient& device, std::span<std::byte> memory, Handle buffer);

    DeviceClient* device_;
    std::span<std::byte> memory_;
    Handle buffer_;
    memory::RingAllocator ring_;
};

} // namespace directmetal::bridge
