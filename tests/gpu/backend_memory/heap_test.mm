#include "device_fixture.hpp"
#include "resource_access.hpp"

#include <map>
#include <vector>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

id<MTLBuffer> metal(Buffer& buffer) {
    return resources::metal_buffer(buffer);
}

TEST(Heaps, PlacesBuffersInPlacementHeaps) {
    const auto device = make_device();
    const auto buffer = make_buffer(*device, 4096);
    ASSERT_NE(buffer, nullptr);
    id<MTLHeap> heap = metal(*buffer).heap;
    ASSERT_NE(heap, nil);
    EXPECT_EQ(heap.type, MTLHeapTypePlacement);
    EXPECT_EQ(heap.hazardTrackingMode, MTLHazardTrackingModeTracked);
    EXPECT_EQ(heap.size, DeviceRequest{}.heap_block_size);
    EXPECT_EQ(buffer->size(), 4096u);
    EXPECT_EQ(metal(*buffer).length, 4096u);
    const MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.heap_count, 1u);
    EXPECT_EQ(stats.heap_bytes, DeviceRequest{}.heap_block_size);
    EXPECT_GE(stats.used_bytes, 4096u);
    EXPECT_EQ(stats.dedicated_count, 0u);
    EXPECT_EQ(stats.pending_free_bytes, 0u);
}

TEST(Heaps, SharedBuffersAreCpuVisibleAndPrivateAreNot) {
    const auto device = make_device();
    const auto shared = make_buffer(*device, 256, StorageMode::shared);
    const auto gpu_private = make_buffer(*device, 256, StorageMode::gpu_private);
    ASSERT_NE(shared->cpu_address(), nullptr);
    EXPECT_EQ(shared->cpu_address(), static_cast<std::byte*>(metal(*shared).contents));
    EXPECT_EQ(gpu_private->cpu_address(), nullptr);
    EXPECT_EQ(metal(*shared).storageMode, MTLStorageModeShared);
    EXPECT_EQ(metal(*gpu_private).storageMode, MTLStorageModePrivate);
    EXPECT_NE(metal(*shared).heap, metal(*gpu_private).heap);
    EXPECT_EQ(device->memory_stats().heap_count, 2u);
}

TEST(Heaps, RejectsMemorylessBuffers) {
    const auto device = make_device();
    const auto buffer = device->create_buffer({256, StorageMode::memoryless, {}});
    ASSERT_FALSE(buffer.has_value());
    EXPECT_EQ(buffer.status(), Status::unsupported);
}

TEST(Heaps, PlacementsInOneHeapDoNotOverlap) {
    const auto device = make_device({.heap_block_size = 4 * mib});
    std::vector<std::unique_ptr<Buffer>> buffers;
    std::map<NSUInteger, NSUInteger> ranges;
    for (std::uint64_t size = 100; size < 200 * kib; size = size * 3 / 2) {
        buffers.push_back(make_buffer(*device, size));
        id<MTLBuffer> placed = metal(*buffers.back());
        ASSERT_EQ(placed.heap, metal(*buffers.front()).heap);
        ranges.emplace(placed.heapOffset, placed.allocatedSize);
    }
    ASSERT_EQ(ranges.size(), buffers.size());
    NSUInteger previous_end = 0;
    for (const auto& [offset, size] : ranges) {
        EXPECT_GE(offset, previous_end);
        previous_end = offset + size;
    }
}

TEST(Heaps, ReusesFreedRangeWhenNoQueueIsTracked) {
    const auto device = make_device();
    auto first = make_buffer(*device, 64 * kib);
    const NSUInteger offset = metal(*first).heapOffset;
    first.reset();
    EXPECT_EQ(device->memory_stats().used_bytes, 0u);
    const auto second = make_buffer(*device, 64 * kib);
    EXPECT_EQ(metal(*second).heapOffset, offset);
}

TEST(Heaps, GrowsByAddingHeaps) {
    const auto device = make_device({.heap_block_size = 1 * mib});
    std::vector<std::unique_ptr<Buffer>> buffers;
    for (int index = 0; index < 3; ++index) {
        buffers.push_back(make_buffer(*device, 512 * kib));
    }
    EXPECT_EQ(metal(*buffers[0]).heap, metal(*buffers[1]).heap);
    EXPECT_NE(metal(*buffers[1]).heap, metal(*buffers[2]).heap);
    const MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.heap_count, 2u);
    EXPECT_EQ(stats.heap_bytes, 2 * mib);
    EXPECT_EQ(stats.used_bytes, 3 * 512 * kib);
}

TEST(Heaps, AddsLargerHeapForResourceAboveBlockSize) {
    const auto device = make_device({.heap_block_size = 1 * mib, .dedicated_threshold = 8 * mib});
    const auto buffer = make_buffer(*device, 3 * mib);
    ASSERT_NE(metal(*buffer).heap, nil);
    EXPECT_GE(metal(*buffer).heap.size, 3 * mib);
    EXPECT_EQ(device->memory_stats().heap_bytes, 3 * mib);
}

TEST(Heaps, AllocatesAtOrAboveThresholdOnItsOwn) {
    const auto device = make_device({.heap_block_size = 1 * mib, .dedicated_threshold = 256 * kib});
    auto below = make_buffer(*device, 128 * kib);
    auto at = make_buffer(*device, 256 * kib);
    EXPECT_NE(metal(*below).heap, nil);
    EXPECT_EQ(metal(*at).heap, nil);
    MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.dedicated_count, 1u);
    EXPECT_EQ(stats.heap_count, 1u);
    EXPECT_EQ(stats.used_bytes, 384 * kib);
    at.reset();
    stats = device->memory_stats();
    EXPECT_EQ(stats.dedicated_count, 0u);
    EXPECT_EQ(stats.used_bytes, 128 * kib);
}

TEST(Heaps, LabelsReachMetalObjects) {
    const auto device = make_device();
    const auto buffer = make_buffer(*device, 256);
    buffer->set_label("vertex data");
    EXPECT_TRUE([metal(*buffer).label isEqualToString:@"vertex data"]);
    const auto texture =
        make_texture(*device, texture_2d(PixelFormat::rgba8_unorm, 8, StorageMode::gpu_private));
    texture->set_label("render target");
    EXPECT_TRUE([resources::metal_texture(*texture).label isEqualToString:@"render target"]);
}

} // namespace
