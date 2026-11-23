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
// The DeviceClient must not move or be destroyed while the ring exists. A moved-from ring is
// invalid except for destruction and release().
class StagingRing {
public:
    struct Allocation {
        std::byte* cpu_address;
        Handle buffer;
        std::uint64_t offset;
    };

    // capacity is rounded up to shared_memory_alignment. Fails with unsupported for a copying
    // import.
    static backend::Result<StagingRing> create(DeviceClient& device, std::uint64_t capacity);

    StagingRing(StagingRing&& other) noexcept;
    StagingRing& operator=(StagingRing&&) = delete;
    StagingRing(const StagingRing&) = delete;
    StagingRing& operator=(const StagingRing&) = delete;
    // Calls release(); if that fails the memory leaks.
    ~StagingRing();

    // Returns nullopt when the ring is full; retire_value is the timeline value whose completion
    // frees the region and must not decrease. alignment is a power of two, raised to at least 4;
    // the base is shared_memory_alignment-aligned, so cpu_address is aligned like offset.
    std::optional<Allocation> allocate(std::uint64_t size, std::uint64_t alignment,
                                       std::uint64_t retire_value);
    void retire(std::uint64_t completed_value);
    // Needs every command naming the buffer submitted and flushed. Idempotent; on failure the
    // memory stays allocated and a later call retries. Afterwards capacity() is 0.
    backend::Status release() noexcept;
    // Bytes imported: the requested capacity rounded up to shared_memory_alignment.
    std::uint64_t capacity() const noexcept;

private:
    StagingRing(DeviceClient& device, std::span<std::byte> memory, Handle buffer);

    DeviceClient* device_;
    std::span<std::byte> memory_;
    Handle buffer_;
    memory::RingAllocator ring_;
};

} // namespace directmetal::bridge
