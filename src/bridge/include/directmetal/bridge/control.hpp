#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/protocol.hpp>

#include <cstddef>
#include <cstdint>

// Parameters of the synchronous control calls, one struct per FunctionCode. Fields marked out are
// written by the unix side.
namespace directmetal::bridge {

inline constexpr std::uint32_t initialize_flag_wow64 = 0x1;

struct alignas(8) InitializeParams {
    // UTF-8 executable name, for logs.
    std::uint64_t client_name;
    std::uint32_t client_name_length;
    std::uint32_t flags;
    std::uint32_t log_level;
    std::uint32_t reserved;
};

// Returns status_no_more_entries once index passes the last adapter.
struct alignas(8) EnumerateAdaptersParams {
    std::uint32_t index;
    std::uint32_t reserved;
    backend::Capabilities adapter; // out
};

struct alignas(8) DeviceOptions {
    std::uint64_t heap_block_size;
    std::uint64_t dedicated_threshold;
};

struct alignas(8) CreateDeviceParams {
    // 0 selects the default device.
    std::uint64_t registry_id;
    DeviceOptions options;
    Handle device;                      // out
    backend::Capabilities capabilities; // out
};

// Drains every stream of the device before destroying it.
struct alignas(8) DestroyDeviceParams {
    Handle device;
};

struct alignas(8) CreateBufferParams {
    Handle device;
    backend::BufferDesc desc;
    Handle buffer; // out
};

// Imports PE-allocated memory from directmetal_bridge_allocate_shared as a buffer without copying.
// address and size must be multiples of shared_memory_alignment, otherwise
// status_invalid_parameter. is_zero_copy is 1 for every accepted import; a copying path for
// unaligned application memory is reserved for later phases. The memory must stay allocated until
// release_memory returns for the buffer; destroy_object on an imported buffer is a stream error.
struct alignas(8) ImportMemoryParams {
    Handle device;
    std::uint64_t address;
    std::uint64_t size;
    Handle buffer;              // out
    std::uint32_t is_zero_copy; // out
    std::uint32_t reserved;
};

struct alignas(8) CreateTextureParams {
    Handle device;
    backend::TextureDesc desc;
    std::uint32_t reserved;
    Handle texture;                // out
    std::uint64_t allocation_size; // out
};

struct alignas(8) CreateTimelineParams {
    Handle device;
    std::uint64_t initial_value;
    // PE-side std::uint64_t the unix side keeps equal to the completed value; 0 for none.
    std::uint64_t mirror_address;
    Handle timeline; // out
};

struct alignas(8) OpenStreamParams {
    Handle device;
    // PE-allocated StreamControl (commands.hpp).
    std::uint64_t control_address;
    Handle stream;            // out
    Handle progress_timeline; // out
};

// Queues a chunk for the stream's consumer thread and returns without waiting for it.
struct alignas(8) SubmitStreamParams {
    Handle stream;
    std::uint64_t chunk_address;
    std::uint32_t chunk_size;
    std::uint32_t reserved;
    std::uint64_t sequence;
};

// Returns status_success or status_timeout. Once the stream that signals the timeline (or whose
// progress timeline it is) has a sticky error, every wait fails with status_device_removed.
struct alignas(8) WaitTimelineParams {
    Handle timeline;
    std::uint64_t value;
    // timeout_infinite waits without a limit.
    std::uint64_t timeout_ns;
};

// Waits for queued chunks to be decoded, then closes the stream.
struct alignas(8) CloseStreamParams {
    Handle stream;
};

// Waits until all GPU work that references the imported buffer has completed, then destroys it.
// Only after this returns may the PE side free the memory.
struct alignas(8) ReleaseMemoryParams {
    Handle buffer;
};

static_assert(sizeof(InitializeParams) == 24);
static_assert(offsetof(InitializeParams, flags) == 12);
static_assert(sizeof(EnumerateAdaptersParams) == 176);
static_assert(offsetof(EnumerateAdaptersParams, adapter) == 8);
static_assert(sizeof(DeviceOptions) == 16);
static_assert(sizeof(CreateDeviceParams) == 200);
static_assert(offsetof(CreateDeviceParams, options) == 8);
static_assert(offsetof(CreateDeviceParams, device) == 24);
static_assert(offsetof(CreateDeviceParams, capabilities) == 32);
static_assert(sizeof(DestroyDeviceParams) == 8);
static_assert(sizeof(CreateBufferParams) == 32);
static_assert(offsetof(CreateBufferParams, desc) == 8);
static_assert(offsetof(CreateBufferParams, buffer) == 24);
static_assert(sizeof(ImportMemoryParams) == 40);
static_assert(offsetof(ImportMemoryParams, address) == 8);
static_assert(offsetof(ImportMemoryParams, size) == 16);
static_assert(offsetof(ImportMemoryParams, buffer) == 24);
static_assert(offsetof(ImportMemoryParams, is_zero_copy) == 32);
static_assert(sizeof(CreateTextureParams) == 64);
static_assert(offsetof(CreateTextureParams, desc) == 8);
static_assert(offsetof(CreateTextureParams, reserved) == 44);
static_assert(offsetof(CreateTextureParams, texture) == 48);
static_assert(offsetof(CreateTextureParams, allocation_size) == 56);
static_assert(sizeof(CreateTimelineParams) == 32);
static_assert(offsetof(CreateTimelineParams, mirror_address) == 16);
static_assert(offsetof(CreateTimelineParams, timeline) == 24);
static_assert(sizeof(OpenStreamParams) == 32);
static_assert(offsetof(OpenStreamParams, control_address) == 8);
static_assert(offsetof(OpenStreamParams, stream) == 16);
static_assert(offsetof(OpenStreamParams, progress_timeline) == 24);
static_assert(sizeof(SubmitStreamParams) == 32);
static_assert(offsetof(SubmitStreamParams, chunk_address) == 8);
static_assert(offsetof(SubmitStreamParams, chunk_size) == 16);
static_assert(offsetof(SubmitStreamParams, sequence) == 24);
static_assert(sizeof(WaitTimelineParams) == 24);
static_assert(offsetof(WaitTimelineParams, value) == 8);
static_assert(offsetof(WaitTimelineParams, timeout_ns) == 16);
static_assert(sizeof(CloseStreamParams) == 8);
static_assert(sizeof(ReleaseMemoryParams) == 8);

} // namespace directmetal::bridge
