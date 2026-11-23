#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/bridge/stream_writer.hpp>
#include <directmetal/formats/formats.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>

namespace directmetal::d3d11 {

// A Direct3D device's unix-side device and its one command stream. The k-th submit signals the
// stream's progress timeline with k. Thread-safe: every stream access holds one lock, so resources
// released on any thread can record their destruction.
class BridgeDevice {
public:
    // Initializes the bridge, creates the default device and opens its stream.
    static backend::Result<std::unique_ptr<BridgeDevice>> open();

    BridgeDevice(const BridgeDevice&) = delete;
    BridgeDevice& operator=(const BridgeDevice&) = delete;
    // Flushes the stream, then destroys the device, which drains and completes every chunk.
    ~BridgeDevice();

    const formats::DeviceFeatures& features() const noexcept;
    backend::Result<bridge::Handle> create_texture(const backend::TextureDesc& desc);
    // memory comes from directmetal_bridge_allocate_shared.
    backend::Result<bridge::Handle> import_memory(std::span<std::byte> memory);
    // Submits recorded work first: the host refuses to release memory while work is unsubmitted.
    // The memory may be freed only when this returns success.
    backend::Status release_memory(bridge::Handle buffer) noexcept;

    // Returns the progress value whose completion covers the command.
    template <typename Command> std::uint64_t record(const Command& command) {
        const std::lock_guard lock(mutex_);
        writer_->record(command);
        has_unsubmitted_ = true;
        return submitted_ + 1;
    }

    // Submits the work `value` covers if it is still recorded, then waits for it; a negative
    // timeout only polls.
    backend::Status wait(std::uint64_t value, std::chrono::nanoseconds timeout);

private:
    explicit BridgeDevice(bridge::DeviceClient client) noexcept;

    backend::Status open_stream();
    void submit_locked();

    bridge::DeviceClient client_;
    formats::DeviceFeatures features_;
    bridge::StreamControl* control_ = nullptr;
    std::optional<bridge::StreamWriter> writer_;
    std::optional<bridge::TimelineClient> progress_;
    std::mutex mutex_;
    std::uint64_t submitted_ = 0;
    bool has_unsubmitted_ = false;
};

} // namespace directmetal::d3d11
