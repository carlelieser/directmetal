#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/device_client.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace directmetal::bridge {

// Upload and readback staging: a ring over shared memory imported once as a unix-side buffer.
// Regions are retired against a timeline's completed value. Used by one thread at a time.
class StagingRing {
public:
    struct Allocation {
        std::byte* cpu_address;
        Handle buffer;
        std::uint64_t offset;
    };

    static backend::Result<StagingRing> create(DeviceClient& device, std::uint64_t capacity);

    // Returns nullopt when the ring is full; retire_value is the timeline value whose completion
    // frees the region.
    std::optional<Allocation> allocate(std::uint64_t size, std::uint64_t alignment,
                                       std::uint64_t retire_value);
    void retire(std::uint64_t completed_value);
};

} // namespace directmetal::bridge
