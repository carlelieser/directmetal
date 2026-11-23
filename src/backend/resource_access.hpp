#pragma once

#ifndef __OBJC__
#error "resource_access.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include <directmetal/backend/device.hpp>

// Private to the backend and only for .mm sources: the Metal objects behind the public interface,
// implemented with the device and resources.
namespace directmetal::backend::resources {

id<MTLDevice> metal_device(Device& device) noexcept;
id<MTLBuffer> metal_buffer(Buffer& buffer) noexcept;
id<MTLTexture> metal_texture(Texture& texture) noexcept;

// Deferred free: a destroyed resource's memory is returned once every tracked queue's progress
// reaches the value that queue last submitted before the destruction (Queue::last_submitted_value,
// Timeline::notify). Queues register on creation and unregister first thing on destruction,
// before any queue member is torn down; a queue must not outlive its device. The device calls
// Queue::last_submitted_value and Queue::progress from the thread destroying a resource.
void track_queue(Device& device, Queue& queue);
void untrack_queue(Device& device, Queue& queue) noexcept;

} // namespace directmetal::backend::resources
