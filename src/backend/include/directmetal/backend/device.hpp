#pragma once

#include <directmetal/backend/commands.hpp>
#include <directmetal/backend/types.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

// The Metal backend's public surface. Objective-C stays inside the backend's .mm sources.
//
// Lifetime: a resource may be destroyed any time after the last submit that references it; the
// backend returns its heap range only once that submit's progress value has completed.
// Threading: Device::create_*, resource destructors and memory_stats are thread-safe; a Queue and
// its recorder belong to one thread at a time; Timeline is thread-safe.
namespace directmetal::backend {

struct MemoryStats {
    std::uint64_t heap_count;
    std::uint64_t heap_bytes;
    std::uint64_t used_bytes;
    std::uint64_t dedicated_count;
    std::uint64_t pending_free_bytes;
};

class Device {
public:
    virtual ~Device() = default;
    virtual const Capabilities& capabilities() const noexcept = 0;
    virtual Result<std::unique_ptr<Buffer>> create_buffer(const BufferDesc& desc) = 0;
    // Wraps caller-owned memory without copying. Its address and size are multiples of
    // import_alignment. The memory must outlive the returned Buffer.
    virtual Result<std::unique_ptr<Buffer>> import_memory(std::span<std::byte> memory) = 0;
    virtual Result<std::unique_ptr<Texture>> create_texture(const TextureDesc& desc) = 0;
    virtual Result<std::unique_ptr<Timeline>> create_timeline(std::uint64_t initial_value) = 0;
    virtual Result<std::unique_ptr<Queue>> create_queue() = 0;
    virtual MemoryStats memory_stats() const = 0;
};

struct DeviceRequest {
    // 0 selects the default device.
    std::uint64_t registry_id = 0;
    std::uint64_t heap_block_size = std::uint64_t{64} << 20;
    // Resources at or above this size get their own allocation instead of a heap range.
    std::uint64_t dedicated_threshold = std::uint64_t{32} << 20;
};

std::vector<Capabilities> enumerate_adapters();
Result<std::unique_ptr<Device>> create_device(const DeviceRequest& request);

} // namespace directmetal::backend
