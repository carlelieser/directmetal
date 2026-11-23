#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// Typed wrappers over the control calls. Bridge failures map to backend::Status; the NTSTATUS
// that caused them is logged.
namespace directmetal::bridge {

// Must succeed before any other call except query_version; repeating it is harmless.
backend::Status initialize(std::string_view client_name, bool is_wow64) noexcept;
backend::Result<std::uint32_t> query_version() noexcept;
backend::Result<std::vector<backend::Capabilities>> enumerate_adapters();

class TimelineClient {
public:
    // mirror is the shared memory passed to create_timeline (or StreamControl::completed_progress).
    TimelineClient(Handle timeline, std::uint64_t* mirror) noexcept;

    Handle handle() const noexcept;
    // Reads the PE-side mirror without a transition.
    std::uint64_t completed() const noexcept;
    // Returns without a transition when the mirror has reached value. A negative timeout does not
    // wait and nanoseconds::max() waits without a limit.
    backend::Status wait(std::uint64_t value, std::chrono::nanoseconds timeout) const noexcept;

private:
    Handle timeline_;
    std::uint64_t* mirror_;
};

struct ImportedMemory {
    Handle buffer;
    bool is_zero_copy;
};

struct OpenedStream {
    Handle stream;
    Handle progress_timeline;
};

class DeviceClient {
public:
    static backend::Result<DeviceClient> create(std::uint64_t registry_id,
                                                const DeviceOptions& options);

    Handle handle() const noexcept;
    const backend::Capabilities& capabilities() const noexcept;
    backend::Result<Handle> create_buffer(const backend::BufferDesc& desc);
    // memory comes from directmetal_bridge_allocate_shared.
    backend::Result<ImportedMemory> import_memory(std::span<std::byte> memory);
    // Returns once GPU work using the buffer has completed; the memory may then be freed.
    backend::Status release_memory(Handle buffer) noexcept;
    backend::Result<Handle> create_texture(const backend::TextureDesc& desc);
    // mirror is 8-byte-aligned shared memory the unix side keeps equal to the completed value; it
    // may be freed once destroy_timeline returns.
    backend::Result<Handle> create_timeline(std::uint64_t initial_value, std::uint64_t& mirror);
    void destroy_timeline(Handle timeline) noexcept;
    // control is 64-byte-aligned shared memory; it may be freed once close_stream or destroy
    // returns.
    backend::Result<OpenedStream> open_stream(StreamControl& control);
    // close_stream, release_memory and destroy require every stream of the device to be flushed
    // first; they return once all recorded work has been submitted and completed on the GPU.
    void close_stream(Handle stream) noexcept;
    // Repeating it is harmless.
    void destroy() noexcept;

private:
    DeviceClient(Handle device, const backend::Capabilities& capabilities) noexcept;

    Handle device_;
    backend::Capabilities capabilities_;
};

} // namespace directmetal::bridge
