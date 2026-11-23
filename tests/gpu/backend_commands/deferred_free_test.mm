#include "commands/test_seams.hpp"
#include "commands_fixture.hpp"
#include "resource_access.hpp"

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

constexpr std::uint64_t small_size = 64 * kib;
constexpr std::uint64_t large_size = 64 * mib;

// Holds the queue's GPU work until opened; opens on destruction so teardown never waits on it.
class Gate {
public:
    explicit Gate(Timeline& timeline) : timeline_(timeline) {}
    Gate(const Gate&) = delete;
    Gate& operator=(const Gate&) = delete;
    ~Gate() { open(); }

    void hold(Queue& queue) {
        directmetal::backend::commands::testing::wait_on_gpu(queue, timeline_, 1);
    }
    void open() { timeline_.signal_cpu(1); }

private:
    Timeline& timeline_;
};

NSUInteger offset_of(Buffer& buffer) {
    return resources::metal_buffer(buffer).heapOffset;
}

using DeferredFree = CommandsTest;

TEST_F(DeferredFree, KeepsRangeUntilQueueCompletes) {
    const auto gate_timeline = make_timeline(0);
    Gate gate(*gate_timeline);
    const auto destination = make_buffer(small_size);
    auto small = make_buffer(small_size);
    const NSUInteger offset = offset_of(*small);
    gate.hold(*queue_);
    recorder().copy_buffer({*small, 0, *destination, 0, small_size});
    ASSERT_EQ(queue_->submit(), Status::success);
    small.reset();
    const auto replacement = make_buffer(small_size);
    EXPECT_NE(offset_of(*replacement), offset);
    EXPECT_EQ(device_->memory_stats().pending_free_bytes, small_size);
    EXPECT_EQ(queue_->progress().wait(1, std::chrono::milliseconds(20)), Status::timeout);
    EXPECT_EQ(device_->memory_stats().pending_free_bytes, small_size);

    gate.open();
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
