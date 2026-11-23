#pragma once

#ifndef __OBJC__
#error "adapters.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include <directmetal/backend/types.hpp>

#include <cstdint>
#include <optional>

namespace directmetal::backend::adapters {

// nullopt when the device is below Apple7 (ADR 0005).
std::optional<Capabilities> describe(id<MTLDevice> device);
// 0 selects the system default device; nil when no device has registry_id.
id<MTLDevice> select(std::uint64_t registry_id);

} // namespace directmetal::backend::adapters
