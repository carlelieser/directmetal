#pragma once

#include "host_context.hpp"
#include "work_queue.hpp"

// The teardown shared by destroy_device, close_stream, release_memory and Host's destructor.
namespace directmetal::host {

// Decodes every chunk queued on every stream of device and waits for the GPU to complete the
// submitted work; see Stream::finish for unsubmitted. False when work stays unsubmitted.
bool finish_streams(HostContext& context, bridge::Handle device, Unsubmitted unsubmitted);
// Removes the object entry names from the table and releases it: a stream's thread is stopped
// and its progress timeline retired, and a timeline's mirror detached, before this returns.
void retire(HostContext& context, const TableEntry& entry) noexcept;
// Finishes the device's streams, then retires every object of the device and the device itself.
void destroy_device_objects(HostContext& context, bridge::Handle device);

} // namespace directmetal::host
