#include "device_fixture.hpp"
#include "fake_queue.hpp"
#include "resource_access.hpp"

#include <thread>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

// Registers a fake queue with the device for the test's duration, as the command lane's queues do.
class TrackedQueue {
public:
    explicit TrackedQueue(Device& device) : device_(device) {
        resources::track_queue(device_, queue_);
    }
    ~TrackedQueue() { resources::untrack_queue(device_, queue_); }

    FakeQueue& operator*() noexcept { return queue_; }
    FakeQueue* operator->() noexcept { return &queue_; }

private:
    Device& device_;
    FakeQueue queue_;
};

constexpr std::uint64_t size = 64 * kib;

NSUInteger offset_of(Buffer& buffer) {
    return resources::metal_buffer(buffer).heapOffset;
}

TEST(DeferredFree, KeepsRangeUntilSubmittedWorkCompletes) {
    const auto device = make_device();
    TrackedQueue queue(*device);
    auto first = make_buffer(*device, size);
    const NSUInteger offset = offset_of(*first);
    queue->submit();
    first.reset();
    MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.used_bytes, 0u);
    EXPECT_EQ(stats.pending_free_bytes, size);
    const auto second = make_buffer(*device, size);
    EXPECT_NE(offset_of(*second), offset);

    queue->fake_progress().signal_cpu(1);
    stats = device->memory_stats();
    EXPECT_EQ(stats.pending_free_bytes, 0u);
    const auto third = make_buffer(*device, size);
    EXPECT_EQ(offset_of(*third), offset);
}

TEST(DeferredFree, WaitsForTheValueSubmittedBeforeDestruction) {
    const auto device = make_device();
    TrackedQueue queue(*device);
    auto buffer = make_buffer(*device, size);
    queue->submit();
    queue->submit();
    buffer.reset();
    queue->submit();
    queue->fake_progress().signal_cpu(1);
    EXPECT_EQ(device->memory_stats().pending_free_bytes, size);
    queue->fake_progress().signal_cpu(2);
    EXPECT_EQ(device->memory_stats().pending_free_bytes, 0u);
}

TEST(DeferredFree, WaitsForEveryTrackedQueue) {
    const auto device = make_device();
    TrackedQueue graphics(*device);
    TrackedQueue copy(*device);
    auto buffer = make_buffer(*device, size);
    graphics->submit();
    copy->submit();
    buffer.reset();
    graphics->fake_progress().signal_cpu(1);
    EXPECT_EQ(device->memory_stats().pending_free_bytes, size);
    copy->fake_progress().signal_cpu(1);
    EXPECT_EQ(device->memory_stats().pending_free_bytes, 0u);
}

TEST(DeferredFree, FreesAtOnceWhenNothingIsOutstanding) {
    const auto device = make_device();
    TrackedQueue idle(*device);
    TrackedQueue completed(*device);
    completed->submit();
    completed->fake_progress().signal_cpu(1);
    auto buffer = make_buffer(*device, size);
    const NSUInteger offset = offset_of(*buffer);
    buffer.reset();
    EXPECT_EQ(idle->fake_progress().waiting_count(), 0u);
    EXPECT_EQ(completed->fake_progress().waiting_count(), 0u);
    EXPECT_EQ(device->memory_stats().pending_free_bytes, 0u);
    EXPECT_EQ(offset_of(*make_buffer(*device, size)), offset);
}

TEST(DeferredFree, IgnoresUntrackedQueues) {
    const auto device = make_device();
    auto buffer = make_buffer(*device, size);
    {
        TrackedQueue retired(*device);
        retired->submit();
    }
    buffer.reset();
    EXPECT_EQ(device->memory_stats().pending_free_bytes, 0u);
}

TEST(DeferredFree, DefersTexturesAndDedicatedAllocations) {
    const auto device = make_device({.heap_block_size = 1 * mib, .dedicated_threshold = 512 * kib});
    TrackedQueue queue(*device);
    auto texture =
        make_texture(*device, texture_2d(PixelFormat::rgba8_unorm, 64, StorageMode::gpu_private));
    auto dedicated = make_buffer(*device, 512 * kib);
    const std::uint64_t used = device->memory_stats().used_bytes;
    queue->submit();
    texture.reset();
    dedicated.reset();
    MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.pending_free_bytes, used);
    EXPECT_EQ(stats.dedicated_count, 1u);
    queue->fake_progress().signal_cpu(1);
    stats = device->memory_stats();
    EXPECT_EQ(stats.pending_free_bytes, 0u);
    EXPECT_EQ(stats.dedicated_count, 0u);
}

TEST(DeferredFree, CompletesFromAnotherThread) {
    const auto device = make_device();
    TrackedQueue queue(*device);
    auto buffer = make_buffer(*device, size);
    const NSUInteger offset = offset_of(*buffer);
    queue->submit();
    buffer.reset();
    std::thread completion([&] { queue->fake_progress().signal_cpu(1); });
    completion.join();
    EXPECT_EQ(offset_of(*make_buffer(*device, size)), offset);
}

TEST(DeferredFree, CompletionAfterDeviceDestructionIsSafe) {
    FakeQueue queue;
    auto device = make_device();
    resources::track_queue(*device, queue);
    auto buffer = make_buffer(*device, size);
    queue.submit();
    buffer.reset();
    resources::untrack_queue(*device, queue);
    device.reset();
    queue.fake_progress().signal_cpu(1);
    EXPECT_EQ(queue.fake_progress().waiting_count(), 0u);
}

} // namespace
