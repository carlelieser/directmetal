#pragma once

#ifndef __OBJC__
#error "metal_options.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include <directmetal/backend/types.hpp>

#include <array>

namespace directmetal::backend::resources {

struct StorageOptions {
    MTLStorageMode storage_mode;
    MTLResourceOptions resource_options;
};

// Indexed by StorageMode.
inline constexpr std::array<StorageOptions, 3> storage_options{{
    {MTLStorageModeShared, MTLResourceStorageModeShared},
    {MTLStorageModePrivate, MTLResourceStorageModePrivate},
    {MTLStorageModeMemoryless, MTLResourceStorageModeMemoryless},
}};

inline MTLStorageMode metal_storage_mode(StorageMode storage) noexcept {
    return storage_options[static_cast<std::size_t>(storage)].storage_mode;
}

inline MTLResourceOptions metal_resource_options(StorageMode storage) noexcept {
    return storage_options[static_cast<std::size_t>(storage)].resource_options;
}

} // namespace directmetal::backend::resources
