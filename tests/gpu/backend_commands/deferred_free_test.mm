#include "commands_fixture.hpp"
#include "resource_access.hpp"

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

constexpr std::uint64_t small_size = 64 * kib;
// Dedicated buffers whose copy keeps the GPU busy for a few milliseconds.
constexpr std::uint64_t large_size = 64 * mib;

NSUInteger offset_of(Buffer& buffer) {
    return resources::metal_buffer(buffer).heapOffset;
}

using DeferredFree = CommandsTest;

TEST_F(DeferredFree, KeepsRangeUntilQueueCompletes) {
    const auto large_source = make_buffer(large_size);
    const auto large_destination = make_buffer(large_size);
    auto small = make_buffer(small_size);
    const NSUInteger offset = offset_of(*small);
    recorder().copy_buffer({*large_source, 0, *large_destination, 0, large_size});
    recorder().copy_buffer({*small, 0, *large_destination, 0, small_size});
    ASSERT_EQ(queue_->submit(), Status::success);
    small.reset();
    const auto replacement = make_buffer(small_size);
    const MemoryStats during = device_->memory_stats();
    const bool is_still_running = queue_->progress().completed_value() == 0;
    if (is_still_running) {
        EXPECT_EQ(during.pending_free_bytes, small_size);
    }
    if (offset_of(*replacement) == offset) {
        EXPECT_GE(queue_->progress().completed_value(), 1u);
    }
    ASSERT_EQ(queue_->progress().wait(1, patience), Status::success);
    EXPECT_EQ(device_->memory_stats().pending_free_bytes, 0u);
    const auto reuse = make_buffer(small_size);
    EXPECT_EQ(offset_of(*reuse), offset);
}

TEST_F(DeferredFree, WaitsForTheSubmitBeforeDestruction) {
    auto buffer = make_buffer(small_size);
    ASSERT_EQ(queue_->submit(), Status::success);
    buffer.reset();
    ASSERT_EQ(queue_->submit(), Status::success);
    ASSERT_EQ(queue_->progress().wait(1, patience), Status::success);
    EXPECT_EQ(device_->memory_stats().pending_free_bytes, 0u);
}

TEST_F(DeferredFree, QueueDestructionCompletesPendingFrees) {
    const auto destination = make_buffer(large_size);
    auto buffer = make_buffer(small_size);
    recorder().copy_buffer({*buffer, 0, *destination, 0, small_size});
    ASSERT_EQ(queue_->submit(), Status::success);
    buffer.reset();
    queue_.reset();
    EXPECT_EQ(device_->memory_stats().pending_free_bytes, 0u);
}

} // namespace
