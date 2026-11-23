#include <directmetal/memory/range_allocator.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <random>
#include <vector>

namespace {

using directmetal::memory::RangeAllocator;

TEST(RangeAllocator, AllocatesFirstFitFromZero) {
    RangeAllocator allocator(1024);
    EXPECT_EQ(allocator.allocate(100, 1), 0u);
    EXPECT_EQ(allocator.allocate(100, 1), 100u);
    EXPECT_EQ(allocator.capacity(), 1024u);
    EXPECT_EQ(allocator.largest_free(), 824u);
}

TEST(RangeAllocator, AlignsOffsets) {
    RangeAllocator allocator(4096);
    ASSERT_EQ(allocator.allocate(3, 1), 0u);
    for (const std::uint64_t alignment : {2u, 16u, 256u, 1024u}) {
        const auto offset = allocator.allocate(5, alignment);
        ASSERT_TRUE(offset.has_value()) << alignment;
        EXPECT_EQ(*offset % alignment, 0u) << alignment;
    }
}

TEST(RangeAllocator, ReusesAlignmentPadding) {
    RangeAllocator allocator(512);
    ASSERT_EQ(allocator.allocate(8, 1), 0u);
    ASSERT_EQ(allocator.allocate(8, 256), 256u);
    EXPECT_EQ(allocator.allocate(200, 8), 8u);
}

TEST(RangeAllocator, ReportsLargestOfSeveralFreeRanges) {
    RangeAllocator allocator(400);
    ASSERT_EQ(allocator.allocate(100, 1), 0u);
    ASSERT_EQ(allocator.allocate(100, 1), 100u);
    ASSERT_EQ(allocator.allocate(50, 1), 200u);
    allocator.free(0);
    EXPECT_EQ(allocator.largest_free(), 150u);
}

TEST(RangeAllocator, ReportsExhaustion) {
    RangeAllocator allocator(256);
    ASSERT_EQ(allocator.allocate(256, 1), 0u);
    EXPECT_EQ(allocator.largest_free(), 0u);
    EXPECT_FALSE(allocator.allocate(1, 1).has_value());
}

TEST(RangeAllocator, RejectsRequestsLargerThanAnyFreeRange) {
    RangeAllocator allocator(300);
    ASSERT_EQ(allocator.allocate(100, 1), 0u);
    ASSERT_EQ(allocator.allocate(100, 1), 100u);
    ASSERT_EQ(allocator.allocate(100, 1), 200u);
    allocator.free(100);
    EXPECT_FALSE(allocator.allocate(101, 1).has_value());
    EXPECT_FALSE(allocator.allocate(100, 128).has_value());
    EXPECT_EQ(allocator.allocate(72, 128), 128u);
    EXPECT_FALSE(RangeAllocator(64).allocate(65, 1).has_value());
}

TEST(RangeAllocator, RejectsZeroSize) {
    RangeAllocator allocator(64);
    EXPECT_FALSE(allocator.allocate(0, 1).has_value());
}

TEST(RangeAllocator, CoalescesWithBothNeighbours) {
    RangeAllocator allocator(300);
    ASSERT_EQ(allocator.allocate(100, 1), 0u);
    ASSERT_EQ(allocator.allocate(100, 1), 100u);
    ASSERT_EQ(allocator.allocate(100, 1), 200u);
    allocator.free(0);
    allocator.free(200);
    EXPECT_EQ(allocator.largest_free(), 100u);
    allocator.free(100);
    EXPECT_EQ(allocator.largest_free(), 300u);
    EXPECT_EQ(allocator.allocate(300, 1), 0u);
}

TEST(RangeAllocator, CoalescesWithPreviousAndNext) {
    RangeAllocator allocator(300);
    ASSERT_EQ(allocator.allocate(100, 1), 0u);
    ASSERT_EQ(allocator.allocate(100, 1), 100u);
    ASSERT_EQ(allocator.allocate(100, 1), 200u);
    allocator.free(0);
    allocator.free(100);
    EXPECT_EQ(allocator.largest_free(), 200u);
    allocator.free(200);
    EXPECT_EQ(allocator.largest_free(), 300u);
}

TEST(RangeAllocator, MovesState) {
    RangeAllocator source(128);
    ASSERT_EQ(source.allocate(64, 1), 0u);
    RangeAllocator moved(std::move(source));
    EXPECT_EQ(moved.allocate(64, 1), 64u);
    RangeAllocator assigned(1);
    assigned = std::move(moved);
    EXPECT_EQ(assigned.capacity(), 128u);
    EXPECT_FALSE(assigned.allocate(1, 1).has_value());
}

// Random allocations and frees never overlap, and freeing everything restores one range.
TEST(RangeAllocator, NeverOverlapsUnderRandomUse) {
    constexpr std::uint64_t capacity = 1 << 20;
    RangeAllocator allocator(capacity);
    std::map<std::uint64_t, std::uint64_t> live;
    std::mt19937_64 random(7);
    for (int step = 0; step < 20000; ++step) {
        const bool is_freeing = !live.empty() && random() % 3 == 0;
        if (is_freeing) {
            auto victim = live.begin();
            std::advance(victim, static_cast<long>(random() % live.size()));
            allocator.free(victim->first);
            live.erase(victim);
            continue;
        }
        const std::uint64_t size = 1 + random() % 4096;
        const std::uint64_t alignment = std::uint64_t{1} << (random() % 10);
        const auto offset = allocator.allocate(size, alignment);
        if (!offset) {
            continue;
        }
        ASSERT_EQ(*offset % alignment, 0u);
        ASSERT_LE(*offset + size, capacity);
        const auto next = live.lower_bound(*offset);
        if (next != live.end()) {
            ASSERT_LE(*offset + size, next->first);
        }
        if (next != live.begin()) {
            const auto previous = std::prev(next);
            ASSERT_LE(previous->first + previous->second, *offset);
        }
        live.emplace(*offset, size);
    }
    for (const auto& [offset, size] : live) {
        allocator.free(offset);
    }
    EXPECT_EQ(allocator.largest_free(), capacity);
}

} // namespace
