#include "adapters.hpp"

#include <directmetal/backend/device.hpp>

#include <algorithm>
#include <array>
#include <cstring>

namespace directmetal::backend::adapters {
namespace {

struct FamilyLimits {
    GpuFamily family;
    MTLGPUFamily metal_family;
    std::uint32_t max_texture_2d;
    std::uint32_t max_texture_3d;
    bool has_bc;
};

// Metal feature set tables; newest first so the first supported entry is the device's family.
// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
constexpr std::array family_limits{
    FamilyLimits{GpuFamily::apple10, MTLGPUFamilyApple10, 16384, 2048, true},
    FamilyLimits{GpuFamily::apple9, MTLGPUFamilyApple9, 16384, 2048, true},
    FamilyLimits{GpuFamily::apple8, MTLGPUFamilyApple8, 16384, 2048, true},
    FamilyLimits{GpuFamily::apple7, MTLGPUFamilyApple7, 16384, 2048, true},
};

const FamilyLimits* find_limits(id<MTLDevice> device) {
    const auto found = std::ranges::find_if(family_limits, [device](const FamilyLimits& limits) {
        return [device supportsFamily:limits.metal_family];
    });
    return found == family_limits.end() ? nullptr : &*found;
}

std::array<char, adapter_name_size> copy_name(NSString* name) {
    std::array<char, adapter_name_size> copied{};
    const char* utf8 = name.UTF8String;
    if (utf8 != nullptr) {
        std::strncpy(copied.data(), utf8, copied.size() - 1);
    }
    return copied;
}

} // namespace

std::optional<Capabilities> describe(id<MTLDevice> device) {
    const FamilyLimits* limits = find_limits(device);
    if (limits == nullptr) {
        return std::nullopt;
    }
    Capabilities capabilities{};
    capabilities.family = limits->family;
    capabilities.has_bc = limits->has_bc && device.supportsBCTextureCompression ? 1 : 0;
    capabilities.has_unified_memory = device.hasUnifiedMemory ? 1 : 0;
    capabilities.max_texture_2d = limits->max_texture_2d;
    capabilities.max_texture_3d = limits->max_texture_3d;
    capabilities.max_buffer_length = device.maxBufferLength;
    capabilities.recommended_working_set = device.recommendedMaxWorkingSetSize;
    capabilities.registry_id = device.registryID;
    capabilities.name = copy_name(device.name);
    return capabilities;
}

id<MTLDevice> select(std::uint64_t registry_id) {
    if (registry_id == 0) {
        return MTLCreateSystemDefaultDevice();
    }
    for (id<MTLDevice> device in MTLCopyAllDevices()) {
        if (device.registryID == registry_id) {
            return device;
        }
    }
    return nil;
}

} // namespace directmetal::backend::adapters

namespace directmetal::backend {

std::vector<Capabilities> enumerate_adapters() {
    std::vector<Capabilities> adapters;
    @autoreleasepool {
        for (id<MTLDevice> device in MTLCopyAllDevices()) {
            if (const auto capabilities = adapters::describe(device)) {
                adapters.push_back(*capabilities);
            }
        }
    }
    return adapters;
}

} // namespace directmetal::backend
