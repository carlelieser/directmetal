#pragma once

#include "mirror.hpp"

#include <directmetal/backend/device.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

// The objects behind handles. Members are declared so that a backend object is destroyed before
// anything it must not outlive.
namespace directmetal::host {

struct DeviceObject {
    explicit DeviceObject(std::unique_ptr<backend::Device> created) : device(std::move(created)) {}

    std::unique_ptr<backend::Device> device;
    // Set before a stream of the device publishes a sticky error; waits on the device's timelines
    // then fail.
    std::atomic<bool> has_failed_stream = false;
    // Serializes resource creation so allocation sizes can be measured.
    std::mutex creation_mutex;
};

struct BufferObject {
    std::unique_ptr<backend::Buffer> buffer;
    std::uint64_t size;
};

struct TextureObject {
    std::unique_ptr<backend::Texture> texture;
    backend::TextureDesc desc;
};

struct TimelineObject {
    std::shared_ptr<DeviceObject> device;
    // For a progress timeline, the stream whose queue owns it.
    std::shared_ptr<void> queue_owner;
    // Null for a progress timeline.
    std::unique_ptr<backend::Timeline> owned;
    backend::Timeline* timeline;
    std::shared_ptr<MirrorWriter> mirror;
    // The highest value a stream has signaled; signals never lower it.
    std::atomic<std::uint64_t> highest_signaled;
};

} // namespace directmetal::host
