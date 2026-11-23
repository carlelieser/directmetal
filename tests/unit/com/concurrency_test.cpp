#include "test_objects.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

constexpr int thread_count = 8;
constexpr int iterations = 100000;

// Starts every thread's work at once, so the counts are contended.
template <typename Work> void run_together(Work work) {
    std::atomic<bool> is_started{false};
    std::vector<std::thread> threads;
    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([&] {
            while (!is_started.load()) {
                std::this_thread::yield();
            }
            work();
        });
    }
    is_started.store(true);
    for (std::thread& thread : threads) {
        thread.join();
    }
}

TEST(ConcurrencyTest, ConcurrentAddRefAndReleaseKeepCounts) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);

    run_together([object] {
        for (int index = 0; index < iterations; ++index) {
            object->AddRef();
            object->add_private_ref();
            object->Release();
            object->release_private();
        }
    });
    EXPECT_EQ(lifetime.destructions, 0);
    EXPECT_EQ(object->AddRef(), 2u);
    EXPECT_EQ(object->Release(), 1u);
    EXPECT_EQ(object->Release(), 0u);
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(ConcurrencyTest, PublicCountCrossingZeroUnderContentionDestroysOnce) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    object->add_private_ref();
    object->Release();

    run_together([object] {
        for (int index = 0; index < iterations; ++index) {
            object->AddRef();
            object->Release();
        }
    });
    EXPECT_EQ(lifetime.destructions, 0);
    EXPECT_EQ(lifetime.first_public_refs, lifetime.last_public_releases - 1);
    object->release_private();
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(ConcurrencyTest, ConcurrentFinalReleasesDestroyOnce) {
    for (int round = 0; round < 1000; ++round) {
        Lifetime lifetime;
        auto* object = new TestObject(lifetime);
        for (int index = 1; index < thread_count; ++index) {
            object->AddRef();
        }

        run_together([object] { object->Release(); });
        ASSERT_EQ(lifetime.destructions, 1);
    }
}

} // namespace
