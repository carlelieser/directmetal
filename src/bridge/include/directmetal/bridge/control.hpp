#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/protocol.hpp>

#include <cstddef>
#include <cstdint>

// Parameters of the synchronous control calls, one struct per FunctionCode. Fields marked out are
// written by the unix side.
//
// Teardown (destroy_device, close_stream, release_memory): the client flushes every stream of the
// device first. The host then decodes every queued chunk of every stream on that device and waits
// for the GPU to complete the submitted work before returning. destroy_device and close_stream
// commit recorded but unsubmitted work first (it signals a progress value the client did not
// count, which no later wait can observe). release_memory never commits implicitly: while any
// stream of the device has recorded work no submit command has committed, it fails with
// status_invalid_device_state and releases nothing; the client records a submit, flushes and
// calls it again. A stream with a sticky error has its recorded work committed, since no later
// submit can reach it.
namespace directmetal::bridge {

inline constexpr std::uint32_t initialize_flag_wow64 = 0x1;

// Must succeed before any call other than query_version and log_call: every other call fails with
// status_invalid_device_state until it has. initialize_flag_wow64 must be set exactly when the call
// comes through Wine's WoW64 call table, otherwise status_invalid_parameter; afterwards every call
// must come through that same table. Repeating it with the same flags is harmless.
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

// Tears down as described above, then destroys every object of the device. Every stream's
// StreamControl may be freed once this returns.
struct alignas(8) DestroyDeviceParams {
    Handle device;
};

struct alignas(8) CreateBufferParams {
    Handle device;
    backend::BufferDesc desc;
    Handle buffer; // out
};

// Imports memory from directmetal_bridge_allocate_shared as a buffer without copying. address and
// size must be multiples of shared_memory_alignment, otherwise status_invalid_parameter.
// is_zero_copy is 1 for every accepted import; a copying path for unaligned application memory is
// reserved for later phases. The memory stays allocated until release_memory returns for the
// buffer; destroy_object on an imported buffer is a stream error.
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
    Handle texture; // out
    // Bytes of device memory the texture occupies, as Metal reports for placing it; 0 for
    // memoryless textures.
    std::uint64_t allocation_size; // out
};

struct alignas(8) CreateTimelineParams {
    Handle device;
    std::uint64_t initial_value;
    // 8-byte-aligned PE-side std::uint64_t the unix side keeps equal to the completed value; 0 for
    // none. It may be freed once destroy_timeline returns.
    std::uint64_t mirror_address;
    Handle timeline; // out
};

// Synchronous: once it returns, the unix side no longer writes the timeline's mirror. Timelines
// are destroyed only this way, never with destroy_object.
struct alignas(8) DestroyTimelineParams {
    Handle timeline;
};

// In Phase 1 a device has at most one open stream: a second open_stream fails with
// status_invalid_device_state until close_stream returns for the first.
struct alignas(8) OpenStreamParams {
    Handle device;
    // 64-byte-aligned PE-side StreamControl (commands.hpp); its completed_progress mirrors the
    // progress timeline. It may be freed once close_stream or destroy_device returns.
    std::uint64_t control_address;
    Handle stream;            // out
    Handle progress_timeline; // out
};

// Queues a chunk for the stream's consumer thread and returns without waiting for it. sequence and
// chunk_size must equal the chunk's ChunkHeader sequence and size, and chunk_size is at most
// max_chunk_size; anything else makes the stream's error sticky. Sequences start at 1 and increase
// by 1 per chunk; StreamControl::retired_sequence (commands.hpp) reports how far decoding got.
struct alignas(8) SubmitStreamParams {
    Handle stream;
    std::uint64_t chunk_address;
    std::uint32_t chunk_size;
    std::uint32_t reserved;
    std::uint64_t sequence;
};

// Returns status_success or status_timeout. Once the stream that signals the timeline (or whose
// progress timeline it is) has a sticky error, every wait fails with status_device_removed. The
// host applies this per device, which is the same thing while a device has one stream (Phase 1).
// A wait still running when destroy_timeline, close_stream or destroy_device retires its timeline
// ends with status_invalid_handle.
struct alignas(8) WaitTimelineParams {
    Handle timeline;
    std::uint64_t value;
    // timeout_infinite waits without a limit.
    std::uint64_t timeout_ns;
};

// Tears down the stream as described above, then closes it.
struct alignas(8) CloseStreamParams {
    Handle stream;
};

// Tears down as described above for the device that owns the imported buffer, then destroys the
// buffer. Only after this returns successfully may the PE side free the memory;
// status_invalid_device_state means unsubmitted work remains and the buffer is still imported.
struct alignas(8) ReleaseMemoryParams {
    Handle buffer;
};

static_assert(sizeof(InitializeParams) == 24);
static_assert(offsetof(InitializeParams, client_name) == 0);
static_assert(offsetof(InitializeParams, client_name_length) == 8);
static_assert(offsetof(InitializeParams, flags) == 12);
static_assert(offsetof(InitializeParams, log_level) == 16);
static_assert(offsetof(InitializeParams, reserved) == 20);
static_assert(sizeof(EnumerateAdaptersParams) == 176);
static_assert(offsetof(EnumerateAdaptersParams, index) == 0);
static_assert(offsetof(EnumerateAdaptersParams, reserved) == 4);
static_assert(offsetof(EnumerateAdaptersParams, adapter) == 8);
static_assert(sizeof(DeviceOptions) == 16);
static_assert(offsetof(DeviceOptions, heap_block_size) == 0);
static_assert(offsetof(DeviceOptions, dedicated_threshold) == 8);
static_assert(sizeof(CreateDeviceParams) == 200);
static_assert(offsetof(CreateDeviceParams, registry_id) == 0);
static_assert(offsetof(CreateDeviceParams, options) == 8);
static_assert(offsetof(CreateDeviceParams, device) == 24);
static_assert(offsetof(CreateDeviceParams, capabilities) == 32);
static_assert(sizeof(DestroyDeviceParams) == 8);
static_assert(sizeof(CreateBufferParams) == 32);
static_assert(offsetof(CreateBufferParams, device) == 0);
static_assert(offsetof(CreateBufferParams, desc) == 8);
static_assert(offsetof(CreateBufferParams, buffer) == 24);
static_assert(sizeof(ImportMemoryParams) == 40);
static_assert(offsetof(ImportMemoryParams, device) == 0);
static_assert(offsetof(ImportMemoryParams, address) == 8);
static_assert(offsetof(ImportMemoryParams, size) == 16);
static_assert(offsetof(ImportMemoryParams, buffer) == 24);
static_assert(offsetof(ImportMemoryParams, is_zero_copy) == 32);
static_assert(offsetof(ImportMemoryParams, reserved) == 36);
static_assert(sizeof(CreateTextureParams) == 64);
static_assert(offsetof(CreateTextureParams, device) == 0);
static_assert(offsetof(CreateTextureParams, desc) == 8);
static_assert(offsetof(CreateTextureParams, reserved) == 44);
static_assert(offsetof(CreateTextureParams, texture) == 48);
static_assert(offsetof(CreateTextureParams, allocation_size) == 56);
static_assert(sizeof(CreateTimelineParams) == 32);
static_assert(offsetof(CreateTimelineParams, device) == 0);
static_assert(offsetof(CreateTimelineParams, initial_value) == 8);
static_assert(offsetof(CreateTimelineParams, mirror_address) == 16);
static_assert(offsetof(CreateTimelineParams, timeline) == 24);
static_assert(sizeof(DestroyTimelineParams) == 8);
static_assert(sizeof(OpenStreamParams) == 32);
static_assert(offsetof(OpenStreamParams, device) == 0);
static_assert(offsetof(OpenStreamParams, control_address) == 8);
static_assert(offsetof(OpenStreamParams, stream) == 16);
static_assert(offsetof(OpenStreamParams, progress_timeline) == 24);
static_assert(sizeof(SubmitStreamParams) == 32);
static_assert(offsetof(SubmitStreamParams, stream) == 0);
static_assert(offsetof(SubmitStreamParams, chunk_address) == 8);
static_assert(offsetof(SubmitStreamParams, chunk_size) == 16);
static_assert(offsetof(SubmitStreamParams, reserved) == 20);
static_assert(offsetof(SubmitStreamParams, sequence) == 24);
static_assert(sizeof(WaitTimelineParams) == 24);
static_assert(offsetof(WaitTimelineParams, timeline) == 0);
static_assert(offsetof(WaitTimelineParams, value) == 8);
static_assert(offsetof(WaitTimelineParams, timeout_ns) == 16);
static_assert(sizeof(CloseStreamParams) == 8);
static_assert(sizeof(ReleaseMemoryParams) == 8);

} // namespace directmetal::bridge
