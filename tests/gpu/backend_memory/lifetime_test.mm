#include "device_fixture.hpp"
#include "resource_access.hpp"

#include <cstdlib>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

// Destroying a resource with no tracked queue releases its Metal object at once, without waiting
// for an autorelease pool the caller may not have.
template <typename Resource, typename Access>
void expect_released(std::unique_ptr<Resource> resource, Access access) {
    __weak id weak;
    @autoreleasepool {
        weak = access(*resource);
    }
    ASSERT_NE(weak, nil);
    resource.reset();
    EXPECT_EQ(weak, nil);
}

TEST(Lifetime, ReleasesPlacedBuffers) {
    const auto device = make_device();
    expect_released(make_buffer(*device, 4096), resources::metal_buffer);
}

TEST(Lifetime, ReleasesDedicatedBuffers) {
    const auto device = make_device({.dedicated_threshold = 1024});
    expect_released(make_buffer(*device, 4096), resources::metal_buffer);
}

TEST(Lifetime, ReleasesTextures) {
    const auto device = make_device();
    expect_released(
        make_texture(*device, texture_2d(PixelFormat::rgba8_unorm, 16, StorageMode::gpu_private)),
        resources::metal_texture);
}

TEST(Lifetime, ReleasesImportedBuffers) {
    const auto device = make_device();
    void* memory = std::aligned_alloc(import_alignment, import_alignment);
    auto imported = device->import_memory({static_cast<std::byte*>(memory), import_alignment});
    ASSERT_TRUE(imported.has_value());
    expect_released(std::move(imported).value(), resources::metal_buffer);
    std::free(memory);
}

} // namespace
