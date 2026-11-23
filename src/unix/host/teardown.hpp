#pragma once

#include "host_context.hpp"

// The teardown shared by destroy_device, close_stream, release_memory and Host's destructor.
namespace directmetal::host {

// Decodes every chunk queued on every stream of device, submits recorded work and waits for the
// GPU to complete it.
void finish_streams(HostContext& context, bridge::Handle device);
// Removes the object entry names from the table and releases it: a stream's thread is stopped
// and its progress timeline retired, and a timeline's mirror detached, before this returns.
void retire(HostContext& context, const TableEntry& entry) noexcept;
// Finishes the device's streams, then retires every object of the device and the device itself.
void destroy_device_objects(HostContext& context, bridge::Handle device);

} // namespace directmetal::host
