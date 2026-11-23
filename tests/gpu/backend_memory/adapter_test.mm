#include "device_fixture.hpp"
#include "resource_access.hpp"

#include <algorithm>
#include <cstring>
#include <set>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

TEST(Adapters, EnumeratesAppleFamilyGpus) {
    const auto adapters = enumerate_adapters();
    ASSERT_FALSE(adapters.empty());
    std::set<std::uint64_t> registry_ids;
    for (const Capabilities& adapter : adapters) {
        EXPECT_GE(adapter.family, GpuFamily::apple7);
        EXPECT_EQ(adapter.has_bc, 1);
        EXPECT_EQ(adapter.has_unified_memory, 1);
        EXPECT_EQ(adapter.max_texture_2d, 16384u);
        EXPECT_EQ(adapter.max_texture_3d, 2048u);
        EXPECT_GT(adapter.max_buffer_length, 256 * mib);
        EXPECT_GT(adapter.recommended_working_set, 0u);
        EXPECT_EQ(adapter.name.back(), '\0');
        EXPECT_GT(std::strlen(adapter.name.data()), 0u);
        EXPECT_EQ(adapter.reserved0, 0);
        EXPECT_EQ(adapter.reserved1, 0u);
        registry_ids.insert(adapter.registry_id);
    }
    EXPECT_EQ(registry_ids.size(), adapters.size());
}

TEST(Adapters, DefaultDeviceIsAnEnumeratedAdapter) {
    const auto device = make_device();
    ASSERT_NE(device, nullptr);
    const auto adapters = enumerate_adapters();
    const auto match =
        std::ranges::find(adapters, device->capabilities().registry_id, &Capabilities::registry_id);
    ASSERT_NE(match, adapters.end());
    EXPECT_EQ(std::strcmp(match->name.data(), device->capabilities().name.data()), 0);
}

TEST(Adapters, CreatesDeviceByRegistryId) {
    for (const Capabilities& adapter : enumerate_adapters()) {
        const auto device = make_device({.registry_id = adapter.registry_id});
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(device->capabilities().registry_id, adapter.registry_id);
        EXPECT_EQ(device->capabilities().family, adapter.family);
        EXPECT_EQ(resources::metal_device(*device).registryID, adapter.registry_id);
    }
}

TEST(Adapters, RejectsUnknownRegistryId) {
    std::uint64_t unknown = 1;
    for (const Capabilities& adapter : enumerate_adapters()) {
        unknown = std::max(unknown, adapter.registry_id + 1);
    }
    const auto device = create_device({.registry_id = unknown});
    ASSERT_FALSE(device.has_value());
    EXPECT_EQ(device.status(), Status::unsupported);
}

TEST(Adapters, ReportsFamilyTheGpuSupports) {
    const auto device = make_device();
    ASSERT_NE(device, nullptr);
    id<MTLDevice> metal = resources::metal_device(*device);
    const auto family = device->capabilities().family;
    const auto metal_family = static_cast<MTLGPUFamily>(
        MTLGPUFamilyApple7 + static_cast<int>(family) - static_cast<int>(GpuFamily::apple7));
    EXPECT_TRUE([metal supportsFamily:metal_family]);
    if (family != GpuFamily::apple10) {
        EXPECT_FALSE([metal supportsFamily:static_cast<MTLGPUFamily>(metal_family + 1)]);
    }
}

} // namespace
