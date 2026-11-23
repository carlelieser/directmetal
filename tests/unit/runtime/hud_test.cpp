#include <directmetal/runtime/hud.hpp>

#include <gtest/gtest.h>

#include <thread>
#include <vector>

namespace {

using namespace directmetal::hud;

class RecordingObserver : public Observer {
public:
    void on_frame(std::span<const std::uint64_t> counters) noexcept override {
        frames.emplace_back(counters.begin(), counters.end());
    }

    std::vector<std::vector<std::uint64_t>> frames;
};

// Counters only grow, so each test measures a difference from where it started.
class HudTest : public testing::Test {
protected:
    void SetUp() override { end_frame(); }
    void TearDown() override { set_observer(nullptr); }
};

TEST_F(HudTest, AddAccumulates) {
    const std::uint64_t before = value(Counter::upload_bytes);
    add(Counter::upload_bytes, 5);
    add(Counter::upload_bytes, 7);
    EXPECT_EQ(value(Counter::upload_bytes) - before, 12u);
}

TEST_F(HudTest, CountersAreIndependent) {
    const std::uint64_t before = value(Counter::submits);
    add(Counter::heap_bytes, 100);
    EXPECT_EQ(value(Counter::submits), before);
}

TEST_F(HudTest, OutOfRangeCounterIsIgnored) {
    add(Counter::count, 1);
    EXPECT_EQ(value(Counter::count), 0u);
}

TEST_F(HudTest, AddsAreAtomicAcrossThreads) {
    constexpr int thread_count = 8;
    constexpr int adds_per_thread = 20000;
    const std::uint64_t before = value(Counter::stream_bytes);
    std::vector<std::thread> threads;
    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([] {
            for (int add_index = 0; add_index < adds_per_thread; ++add_index) {
                add(Counter::stream_bytes, 3);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(value(Counter::stream_bytes) - before,
              static_cast<std::uint64_t>(thread_count) * adds_per_thread * 3);
}

TEST_F(HudTest, EndFrameReportsAmountsSincePreviousFrame) {
    RecordingObserver observer;
    set_observer(&observer);
    add(Counter::control_calls, 4);
    end_frame();
    add(Counter::control_calls, 1);
    add(Counter::stream_flushes, 2);
    end_frame();
    end_frame();
    ASSERT_EQ(observer.frames.size(), 3u);
    const auto control = static_cast<std::size_t>(Counter::control_calls);
    const auto flushes = static_cast<std::size_t>(Counter::stream_flushes);
    EXPECT_EQ(observer.frames[0].size(), static_cast<std::size_t>(Counter::count));
    EXPECT_EQ(observer.frames[0][control], 4u);
    EXPECT_EQ(observer.frames[1][control], 1u);
    EXPECT_EQ(observer.frames[1][flushes], 2u);
    EXPECT_EQ(observer.frames[2][control], 0u);
    EXPECT_EQ(observer.frames[2][flushes], 0u);
}

TEST_F(HudTest, EndFrameDoesNotResetTotals) {
    const std::uint64_t before = value(Counter::submits);
    add(Counter::submits, 2);
    end_frame();
    EXPECT_EQ(value(Counter::submits) - before, 2u);
}

TEST_F(HudTest, FramesWithoutObserverAreNotReportedLater) {
    add(Counter::heap_bytes, 9);
    end_frame();
    RecordingObserver observer;
    set_observer(&observer);
    end_frame();
    ASSERT_EQ(observer.frames.size(), 1u);
    EXPECT_EQ(observer.frames[0][static_cast<std::size_t>(Counter::heap_bytes)], 0u);
}

TEST_F(HudTest, RemovedObserverIsNotCalled) {
    RecordingObserver observer;
    set_observer(&observer);
    set_observer(nullptr);
    end_frame();
    EXPECT_TRUE(observer.frames.empty());
}

TEST_F(HudTest, ConcurrentAddsAreAllReportedAcrossFrames) {
    RecordingObserver observer;
    set_observer(&observer);
    constexpr int adds = 50000;
    std::thread adder([] {
        for (int index = 0; index < adds; ++index) {
            add(Counter::upload_bytes, 1);
        }
    });
    for (int frame = 0; frame < 50; ++frame) {
        end_frame();
    }
    adder.join();
    end_frame();
    std::uint64_t reported = 0;
    for (const auto& frame : observer.frames) {
        reported += frame[static_cast<std::size_t>(Counter::upload_bytes)];
    }
    EXPECT_EQ(reported, static_cast<std::uint64_t>(adds));
}

} // namespace
