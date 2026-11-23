#pragma once

#include <directmetal/backend/device.hpp>

#include <cstdint>
#include <memory>

// Private to the backend. Implemented by src/backend/commands; Device::create_queue and
// Device::create_timeline delegate here.
namespace directmetal::backend::commands {

Result<std::unique_ptr<Queue>> create_queue(Device& device);
Result<std::unique_ptr<Timeline>> create_timeline(Device& device, std::uint64_t initial_value);

} // namespace directmetal::backend::commands
