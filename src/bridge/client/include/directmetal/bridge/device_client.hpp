#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/control.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

// Typed wrappers over the control calls. Bridge failures map to backend::Status; the NTSTATUS
// that caused them is logged.
namespace directmetal::bridge {

class TimelineClient {
public:
    TimelineClient(Handle timeline, const std::uint64_t& mirror) noexcept;

    Handle handle() const noexcept;
    // Reads the PE-side mirror without a transition.
    std::uint64_t completed() const noexcept;
    backend::Status wait(std::uint64_t value, std::chrono::nanoseconds timeout) const noexcept;
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
    // mirror is PE-side memory the unix side keeps equal to the completed value.
    backend::Result<Handle> create_timeline(std::uint64_t initial_value, std::uint64_t& mirror);
    backend::Result<OpenedStream> open_stream(StreamControl& control);
    void close_stream(Handle stream) noexcept;
    void destroy() noexcept;
};

} // namespace directmetal::bridge
