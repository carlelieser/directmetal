#include "commands/fault_injection.hpp"
#include "commands_fixture.hpp"

#include <atomic>
#include <thread>

// Metal command buffer errors are not provoked: fail_next_submit marks a submit's command buffer
// failed in the completion path, which then takes the same device-lost route as a real error.
namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

using DeviceLost = CommandsTest;

TEST_F(DeviceLost, FailedSubmitReleasesWaitersAndRefusesLaterSubmits) {
    std::atomic<Status> waited = Status::success;
    std::thread waiter([&] { waited = queue_->progress().wait(100, patience); });
    directmetal::backend::commands::testing::fail_next_submit(*queue_);
    ASSERT_EQ(queue_->submit(), Status::success);
    waiter.join();
    EXPECT_EQ(waited.load(), Status::device_lost);
    EXPECT_EQ(queue_->progress().wait(2, patience), Status::device_lost);
    EXPECT_EQ(queue_->submit(), Status::device_lost);
    EXPECT_EQ(queue_->last_submitted_value(), 1u);
}

TEST_F(DeviceLost, RunsPendingAndLaterNotifyCallbacks) {
    std::atomic<int> runs = 0;
    queue_->progress().notify(5, [&] { ++runs; });
    directmetal::backend::commands::testing::fail_next_submit(*queue_);
    ASSERT_EQ(queue_->submit(), Status::success);
    ASSERT_EQ(queue_->progress().wait(5, patience), Status::device_lost);
    EXPECT_EQ(runs.load(), 1);
    queue_->progress().notify(7, [&] { ++runs; });
    EXPECT_EQ(runs.load(), 2);
}

TEST_F(DeviceLost, ReleasesTimelinesTheQueueSignaled) {
    const auto timeline = make_timeline(0);
    recorder().signal(*timeline, 1);
    directmetal::backend::commands::testing::fail_next_submit(*queue_);
    ASSERT_EQ(queue_->submit(), Status::success);
    EXPECT_EQ(timeline->wait(2, patience), Status::device_lost);
}

TEST_F(DeviceLost, CompletesDeferredFrees) {
    auto buffer = make_filled_buffer(64 * kib, 0);
    const auto destination = make_buffer(64 * kib);
    recorder().copy_buffer({*buffer, 0, *destination, 0, 64 * kib});
    directmetal::backend::commands::testing::fail_next_submit(*queue_);
    ASSERT_EQ(queue_->submit(), Status::success);
    buffer.reset();
    ASSERT_EQ(queue_->progress().wait(2, patience), Status::device_lost);
    EXPECT_EQ(device_->memory_stats().pending_free_bytes, 0u);
}

TEST_F(DeviceLost, OtherQueuesKeepWorking) {
    auto other = device_->create_queue();
    ASSERT_TRUE(other.has_value());
    directmetal::backend::commands::testing::fail_next_submit(*queue_);
    ASSERT_EQ(queue_->submit(), Status::success);
    ASSERT_EQ(queue_->progress().wait(2, patience), Status::device_lost);
    ASSERT_EQ(other.value()->submit(), Status::success);
    EXPECT_EQ(other.value()->progress().wait(1, patience), Status::success);
}

} // namespace
