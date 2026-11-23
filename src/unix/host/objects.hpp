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
    // Held while an object of the device is created and inserted, and while it is marked
    // destroyed, so destroy_device sees every object created before it.
    std::mutex lifecycle_mutex;
    // Set once, under lifecycle_mutex, when destroy_device starts.
    std::atomic<bool> is_destroyed = false;
};

// Device's lifecycle lock, or a lock that owns nothing once the device is destroyed.
inline std::unique_lock<std::mutex> lock_if_live(DeviceObject& device) {
    std::unique_lock lock(device.lifecycle_mutex);
    if (device.is_destroyed.load()) {
        lock.unlock();
    }
    return lock;
}

// False if another call already marked the device destroyed.
inline bool mark_destroyed(DeviceObject& device) {
    const std::lock_guard lock(device.lifecycle_mutex);
    return !device.is_destroyed.exchange(true);
}

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
