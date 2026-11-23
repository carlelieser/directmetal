#pragma once

#include <directmetal/backend/device.hpp>

#include <cstdint>
#include <memory>

// Private to the backend. Implemented by src/backend/commands; Device::create_queue and
// Device::create_timeline delegate here.
//
// A queue calls resources::track_queue when created and resources::untrack_queue first in its
// destructor, before any of its members is torn down; it must not outlive its Device. A timeline
// runs every pending Timeline::notify callback when it is destroyed or the device is lost.
namespace directmetal::backend::commands {

Result<std::unique_ptr<Queue>> create_queue(Device& device);
Result<std::unique_ptr<Timeline>> create_timeline(Device& device, std::uint64_t initial_value);

} // namespace directmetal::backend::commands
