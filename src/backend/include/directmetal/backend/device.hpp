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
// backend returns its heap range only once that submit's progress value has completed. Imported
// buffers are not deferred: the caller's memory must outlive all GPU work submitted against the
// imported Buffer, which the host's release_memory guarantees by waiting for the GPU. Resources
// may outlive their Device; after the Device is destroyed, the last completion callback of a
// pending free may release Metal objects on a Metal completion thread, which is acceptable.
// Threading: Device::create_*, resource destructors and memory_stats are thread-safe; a Queue and
// its recorder belong to one thread at a time; Timeline is thread-safe.
// Validation: callers pass valid arguments. Front ends validate against the Direct3D rules and the
// host validates every decoded command; the backend only asserts, in debug builds.
namespace directmetal::backend {

// Imported buffers are not counted. Sizes are those Metal reports for heap placement.
struct MemoryStats {
    // Heaps of every storage mode, and their total size.
    std::uint64_t heap_count;
    std::uint64_t heap_bytes;
    // Live buffers and textures, placed or dedicated; memoryless textures count 0 bytes.
    std::uint64_t used_bytes;
    // Dedicated allocations still held: live, or destroyed and awaiting completion.
    std::uint64_t dedicated_count;
    // Destroyed resources whose memory awaits completion of submitted work.
    std::uint64_t pending_free_bytes;
};

class Device {
public:
    virtual ~Device() = default;
    virtual const Capabilities& capabilities() const noexcept = 0;
    // StorageMode::memoryless returns Status::unsupported.
    virtual Result<std::unique_ptr<Buffer>> create_buffer(const BufferDesc& desc) = 0;
    // Wraps caller-owned memory without copying. Its address and size are multiples of
    // import_alignment. The memory must outlive the returned Buffer and all GPU work submitted
    // against it. Memory Metal rejects, such as inaccessible pages, returns
    // Status::invalid_argument.
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

// Apple7 and later GPUs only (ADR 0005).
std::vector<Capabilities> enumerate_adapters();
// Status::unsupported when no device has request.registry_id or the device is below Apple7.
Result<std::unique_ptr<Device>> create_device(const DeviceRequest& request);

} // namespace directmetal::backend
