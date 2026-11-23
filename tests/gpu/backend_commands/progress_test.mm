#include "commands_fixture.hpp"

#include <atomic>
#include <thread>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

TEST_F(CommandsTest, ProgressStartsAtZero) {
    EXPECT_EQ(queue_->last_submitted_value(), 0u);
    EXPECT_EQ(queue_->progress().completed_value(), 0u);
    EXPECT_EQ(queue_->progress().wait(0, std::chrono::nanoseconds{0}), Status::success);
}

TEST_F(CommandsTest, KthSubmitSignalsK) {
    Timeline* const progress = &queue_->progress();
    for (std::uint64_t submit = 1; submit <= 5; ++submit) {
        ASSERT_EQ(queue_->submit(), Status::success);
        EXPECT_EQ(queue_->last_submitted_value(), submit);
        EXPECT_EQ(&queue_->progress(), progress);
    }
    for (std::uint64_t submit = 1; submit <= 5; ++submit) {
        EXPECT_EQ(progress->wait(submit, patience), Status::success);
    }
    EXPECT_EQ(progress->completed_value(), 5u);
    EXPECT_EQ(progress->wait(6, std::chrono::milliseconds(20)), Status::timeout);
}

// Other threads read the queue's counters while its owner submits.
TEST_F(CommandsTest, CountersReadConcurrentlyStayOrdered) {
    constexpr std::uint64_t submits = 50;
    std::atomic<bool> is_done = false;
    std::atomic<bool> is_ordered = true;
    std::thread reader([&] {
        std::uint64_t last_seen = 0;
        while (!is_done.load()) {
            const std::uint64_t completed = queue_->progress().completed_value();
            const std::uint64_t submitted = queue_->last_submitted_value();
            const bool is_monotonic = completed >= last_seen;
            is_ordered = is_ordered && is_monotonic && completed <= submitted;
            last_seen = completed;
        }
    });
    for (std::uint64_t submit = 0; submit < submits; ++submit) {
        ASSERT_EQ(queue_->submit(), Status::success);
    }
    EXPECT_EQ(queue_->progress().wait(submits, patience), Status::success);
    is_done = true;
    reader.join();
    EXPECT_TRUE(is_ordered.load());
}

TEST_F(CommandsTest, DestroyingQueueWaitsForSubmittedWork) {
    constexpr std::uint64_t size = 16 * mib;
    const auto source = make_filled_buffer(size, 0x3c);
    const auto destination = make_filled_buffer(size, 0);
    recorder().copy_buffer({*source, 0, *destination, 0, size});
    ASSERT_EQ(queue_->submit(), Status::success);
    queue_.reset();
    EXPECT_EQ(std::memcmp(source->cpu_address(), destination->cpu_address(), size), 0);
}

TEST_F(CommandsTest, DestroyingQueueDropsUnsubmittedWork) {
    const auto source = make_filled_buffer(kib, 0x3c);
    const auto destination = make_filled_buffer(kib, 0);
    recorder().copy_buffer({*source, 0, *destination, 0, kib});
    queue_.reset();
    EXPECT_EQ(std::to_integer<int>(destination->cpu_address()[0]), 0);
}

} // namespace
