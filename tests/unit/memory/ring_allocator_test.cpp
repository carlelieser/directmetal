#include <directmetal/memory/ring_allocator.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using directmetal::memory::RingAllocator;

TEST(RingAllocator, AllocatesContiguouslyInOrder) {
    RingAllocator ring(1024);
    EXPECT_EQ(ring.allocate(100, 1, 1), 0u);
    EXPECT_EQ(ring.allocate(100, 1, 1), 100u);
    EXPECT_EQ(ring.allocate(100, 64, 2), 256u);
    EXPECT_EQ(ring.capacity(), 1024u);
}

TEST(RingAllocator, ReportsFullRing) {
    RingAllocator ring(256);
    ASSERT_EQ(ring.allocate(200, 1, 1), 0u);
    EXPECT_FALSE(ring.allocate(57, 1, 2).has_value());
    EXPECT_EQ(ring.allocate(56, 1, 2), 200u);
    EXPECT_FALSE(ring.allocate(1, 1, 2).has_value());
}

TEST(RingAllocator, RejectsZeroAndOversizeRequests) {
    RingAllocator ring(256);
    EXPECT_FALSE(ring.allocate(0, 1, 1).has_value());
    EXPECT_FALSE(ring.allocate(257, 1, 1).has_value());
    EXPECT_EQ(ring.allocate(256, 1, 1), 0u);
}

TEST(RingAllocator, RetiresUpToCompletedValue) {
    RingAllocator ring(300);
    ASSERT_EQ(ring.allocate(100, 1, 1), 0u);
    ASSERT_EQ(ring.allocate(100, 1, 2), 100u);
    ASSERT_EQ(ring.allocate(100, 1, 3), 200u);
    ring.retire(0);
    EXPECT_FALSE(ring.allocate(1, 1, 4).has_value());
    ring.retire(2);
    EXPECT_EQ(ring.allocate(200, 1, 4), 0u);
    EXPECT_FALSE(ring.allocate(1, 1, 4).has_value());
}

TEST(RingAllocator, WrapsToStartWhenTailIsTooShort) {
    RingAllocator ring(256);
    ASSERT_EQ(ring.allocate(100, 1, 1), 0u);
    ASSERT_EQ(ring.allocate(100, 1, 2), 100u);
    ring.retire(1);
    // 56 bytes remain before the end; the region wraps to 0, which the first retire freed.
    EXPECT_EQ(ring.allocate(80, 1, 3), 0u);
    EXPECT_FALSE(ring.allocate(21, 1, 3).has_value());
    EXPECT_EQ(ring.allocate(20, 1, 3), 80u);
}

TEST(RingAllocator, WrapsWhenAlignmentPassesTheEnd) {
    RingAllocator ring(256);
    ASSERT_EQ(ring.allocate(200, 1, 1), 0u);
    ASSERT_EQ(ring.allocate(10, 1, 2), 200u);
    ring.retire(1);
    EXPECT_EQ(ring.allocate(16, 64, 3), 0u);
}

TEST(RingAllocator, SkippedTailIsHeldUntilItsRegionRetires) {
    RingAllocator ring(256);
    ASSERT_EQ(ring.allocate(200, 1, 1), 0u);
    ASSERT_EQ(ring.allocate(10, 1, 2), 200u);
    ring.retire(1);
    ASSERT_EQ(ring.allocate(100, 1, 3), 0u);
    ring.retire(2);
    // The region at 0 also owns bytes 210..255, so only 100..209 is free.
    EXPECT_FALSE(ring.allocate(111, 1, 4).has_value());
    EXPECT_EQ(ring.allocate(110, 1, 4), 100u);
}

TEST(RingAllocator, RestartsAtZeroWhenEmpty) {
    RingAllocator ring(256);
    ASSERT_EQ(ring.allocate(100, 1, 1), 0u);
    ring.retire(1);
    EXPECT_EQ(ring.allocate(256, 1, 2), 0u);
}

TEST(RingAllocator, ReleasesInAllocationOrderOverManyLaps) {
    RingAllocator ring(1000);
    std::uint64_t expected = 0;
    for (std::uint64_t value = 1; value <= 1000; ++value) {
        const auto offset = ring.allocate(300, 1, value);
        ASSERT_TRUE(offset.has_value()) << value;
        ASSERT_EQ(*offset, expected) << value;
        expected = expected + 600 > 1000 ? 0 : expected + 300;
        ring.retire(value - 1);
    }
}

TEST(RingAllocator, MovesState) {
    RingAllocator source(128);
    ASSERT_EQ(source.allocate(64, 1, 1), 0u);
    RingAllocator moved(std::move(source));
    EXPECT_EQ(moved.allocate(64, 1, 1), 64u);
    EXPECT_FALSE(moved.allocate(1, 1, 1).has_value());
}

} // namespace
