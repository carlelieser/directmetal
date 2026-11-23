#include "commands_fixture.hpp"

#include <atomic>
#include <thread>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;
using namespace std::chrono_literals;

constexpr std::chrono::nanoseconds no_wait{0};
// timeout_infinite of the bridge protocol, as the host passes it on.
constexpr std::chrono::nanoseconds unbounded = std::chrono::nanoseconds::max();

using TimelineTest = CommandsTest;

TEST_F(TimelineTest, StartsAtInitialValue) {
    const auto timeline = make_timeline(7);
    EXPECT_EQ(timeline->completed_value(), 7u);
    EXPECT_EQ(timeline->wait(7, no_wait), Status::success);
    EXPECT_EQ(timeline->wait(8, no_wait), Status::timeout);
}

TEST_F(TimelineTest, SignalCpuCompletesValue) {
    const auto timeline = make_timeline(0);
    timeline->signal_cpu(4);
    EXPECT_EQ(timeline->completed_value(), 4u);
    EXPECT_EQ(timeline->wait(4, patience), Status::success);
    EXPECT_EQ(timeline->wait(3, no_wait), Status::success);
}

TEST_F(TimelineTest, WaitTimesOut) {
    const auto timeline = make_timeline(0);
    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(timeline->wait(1, 20ms), Status::timeout);
    EXPECT_GE(std::chrono::steady_clock::now() - start, 20ms);
}

TEST_F(TimelineTest, UnboundedWaitReturnsOnSignal) {
    const auto timeline = make_timeline(0);
    std::atomic<Status> result = Status::invalid_argument;
    std::thread waiter([&] { result = timeline->wait(3, unbounded); });
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(result.load(), Status::invalid_argument);
    timeline->signal_cpu(3);
    waiter.join();
    EXPECT_EQ(result.load(), Status::success);
}

TEST_F(TimelineTest, RecorderSignalsOnGpu) {
    const auto timeline = make_timeline(2);
    recorder().signal(*timeline, 9);
    ASSERT_EQ(queue_->submit(), Status::success);
    EXPECT_EQ(timeline->wait(9, patience), Status::success);
    EXPECT_EQ(timeline->completed_value(), 9u);
}

TEST_F(TimelineTest, NotifyRunsImmediatelyWhenReached) {
    const auto timeline = make_timeline(5);
    bool has_run = false;
    timeline->notify(5, [&] { has_run = true; });
    EXPECT_TRUE(has_run);
}

TEST_F(TimelineTest, NotifyRunsOnCompletionThreadOnceValueIsReached) {
    const auto timeline = make_timeline(0);
    std::atomic<bool> has_first_run = false;
    std::atomic<bool> has_second_run = false;
    std::atomic<std::thread::id> runner;
    timeline->notify(1, [&] { has_first_run = true; });
    timeline->notify(2, [&] {
        runner = std::this_thread::get_id();
        has_second_run = true;
    });
    timeline->signal_cpu(1);
    ASSERT_EQ(timeline->wait(1, patience), Status::success);
    EXPECT_TRUE(has_first_run.load());
    EXPECT_FALSE(has_second_run.load());
    timeline->signal_cpu(2);
    ASSERT_EQ(timeline->wait(2, patience), Status::success);
    EXPECT_TRUE(has_second_run.load());
    EXPECT_NE(runner.load(), std::this_thread::get_id());
}

TEST_F(TimelineTest, NotifyFollowsGpuSignal) {
    const auto timeline = make_timeline(0);
    std::atomic<int> runs = 0;
    timeline->notify(1, [&] { ++runs; });
    recorder().signal(*timeline, 1);
    ASSERT_EQ(queue_->submit(), Status::success);
    ASSERT_EQ(timeline->wait(1, patience), Status::success);
    EXPECT_EQ(runs.load(), 1);
}

TEST_F(TimelineTest, DestructionRunsPendingCallbacks) {
    auto timeline = make_timeline(0);
    std::atomic<int> runs = 0;
    timeline->notify(10, [&] { ++runs; });
    timeline->notify(20, [&] { ++runs; });
    timeline.reset();
    EXPECT_EQ(runs.load(), 2);
}

TEST_F(TimelineTest, LateSignalAfterDestructionRunsNothingTwice) {
    auto timeline = make_timeline(0);
    std::atomic<int> runs = 0;
    timeline->notify(1, [&] { ++runs; });
    recorder().signal(*timeline, 1);
    timeline.reset();
    submit_and_wait();
    EXPECT_EQ(runs.load(), 1);
}

} // namespace

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

// A callback slow enough that a wait or destruction racing it would observe it unfinished.
struct SlowCallback {
    std::atomic<bool> has_started = false;
    std::atomic<bool> has_finished = false;

    std::function<void()> callback() {
        return [this] {
            has_started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            has_finished = true;
        };
    }

    bool wait_started() const {
        const auto deadline = std::chrono::steady_clock::now() + patience;
        while (!has_started.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        return has_started.load();
    }
};

using RunningCallback = CommandsTest;

TEST_F(RunningCallback, WaitReturnsAfterItFinishes) {
    const auto timeline = make_timeline(0);
    SlowCallback slow;
    timeline->notify(1, slow.callback());
    std::thread signaler([&] { timeline->signal_cpu(1); });
    EXPECT_TRUE(slow.wait_started());
    EXPECT_EQ(timeline->wait(1, std::chrono::nanoseconds{0}), Status::timeout);
    EXPECT_EQ(timeline->wait(1, patience), Status::success);
    EXPECT_TRUE(slow.has_finished.load());
    signaler.join();
}

TEST_F(RunningCallback, DestructionWaitsForIt) {
    auto timeline = make_timeline(0);
    SlowCallback slow;
    timeline->notify(1, slow.callback());
    recorder().signal(*timeline, 1);
    ASSERT_EQ(queue_->submit(), Status::success);
    ASSERT_TRUE(slow.wait_started());
    timeline.reset();
    EXPECT_TRUE(slow.has_finished.load());
}

} // namespace
