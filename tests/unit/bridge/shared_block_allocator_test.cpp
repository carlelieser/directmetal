#include "fake_transport.hpp"

#include <directmetal/bridge/shared_block_allocator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;

std::uint64_t address_of(const void* block) {
    return reinterpret_cast<std::uintptr_t>(block);
}

bool is_filled(const void* block, std::uint64_t size, std::byte value) {
    const auto* bytes = static_cast<const std::byte*>(block);
    return std::all_of(bytes, bytes + size, [value](std::byte item) { return item == value; });
}

class SharedBlockAllocatorTest : public testing::Test {
protected:
    void SetUp() override { reset_shared_memory_stats(); }
    void TearDown() override { fail_shared_allocations(false); }
};

TEST_F(SharedBlockAllocatorTest, AlignsAndZeroesBlocks) {
    SharedBlockAllocator allocator;
    for (const std::uint64_t alignment : {1, 8, 64, 256, 4096}) {
        for (const std::uint64_t size : {1, 8, 13, 64, 1000}) {
            void* block = allocator.allocate(size, alignment);
            ASSERT_NE(block, nullptr) << size << " " << alignment;
            EXPECT_EQ(address_of(block) % alignment, 0u) << size << " " << alignment;
            EXPECT_TRUE(is_filled(block, size, std::byte{0})) << size << " " << alignment;
            std::memset(block, 0xFF, size);
        }
    }
}

TEST_F(SharedBlockAllocatorTest, ZeroesReusedBlocks) {
    SharedBlockAllocator allocator;
    void* first = allocator.allocate(64, 64);
    std::memset(first, 0xFF, 64);
    allocator.free(first, 64);
    void* second = allocator.allocate(64, 64);
    EXPECT_EQ(second, first);
    EXPECT_TRUE(is_filled(second, 64, std::byte{0}));
}

TEST_F(SharedBlockAllocatorTest, KeepsBlocksDisjoint) {
    SharedBlockAllocator allocator;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> blocks;
    for (std::uint64_t index = 0; index < 400; ++index) {
        const std::uint64_t size = 1 + (index * 37) % 300;
        const std::uint64_t alignment = std::uint64_t{1} << (index % 8);
        const std::uint64_t address = address_of(allocator.allocate(size, alignment));
        ASSERT_NE(address, 0u);
        blocks.emplace_back(address, address + size);
    }
    std::sort(blocks.begin(), blocks.end());
    for (std::size_t index = 1; index < blocks.size(); ++index) {
        EXPECT_LE(blocks[index - 1].second, blocks[index].first) << "block " << index;
    }
}

TEST_F(SharedBlockAllocatorTest, CarvesControlBlocksAndMirrorsFromOneRegion) {
    SharedBlockAllocator allocator;
    for (int index = 0; index < 100; ++index) {
        const StreamControl* control = allocator.allocate_stream_control();
        ASSERT_NE(control, nullptr);
        EXPECT_EQ(address_of(control) % 64, 0u);
        EXPECT_TRUE(is_filled(control, sizeof(StreamControl), std::byte{0}));
        const std::uint64_t* mirror = allocator.allocate_mirror();
        ASSERT_NE(mirror, nullptr);
        EXPECT_EQ(address_of(mirror) % 8, 0u);
        EXPECT_EQ(*mirror, 0u);
    }
    EXPECT_EQ(shared_memory_stats().total, 1u);
}

TEST_F(SharedBlockAllocatorTest, CoalescesFreedRanges) {
    SharedBlockAllocator allocator;
    constexpr std::uint64_t piece = SharedBlockAllocator::region_size / 8;
    std::vector<void*> pieces;
    for (int index = 0; index < 8; ++index) {
        pieces.push_back(allocator.allocate(piece, 8));
    }
    for (const std::size_t index : {1, 3, 0, 2, 7, 5, 6, 4}) {
        allocator.free(pieces[index], piece);
    }
    EXPECT_NE(allocator.allocate(SharedBlockAllocator::region_size, 8), nullptr);
    EXPECT_EQ(shared_memory_stats().total, 1u);
}

TEST_F(SharedBlockAllocatorTest, AddsRegionsWhenFull) {
    SharedBlockAllocator allocator;
    EXPECT_NE(allocator.allocate(SharedBlockAllocator::region_size, 8), nullptr);
    EXPECT_NE(allocator.allocate(8, 8), nullptr);
    EXPECT_EQ(shared_memory_stats().total, 2u);
}

TEST_F(SharedBlockAllocatorTest, RejectsInvalidRequests) {
    SharedBlockAllocator allocator;
    EXPECT_EQ(allocator.allocate(0, 8), nullptr);
    EXPECT_EQ(allocator.allocate(SharedBlockAllocator::region_size + 1, 8), nullptr);
    EXPECT_EQ(allocator.allocate(8, 0), nullptr);
    EXPECT_EQ(allocator.allocate(8, 24), nullptr);
    EXPECT_EQ(allocator.allocate(8, 8192), nullptr);
    EXPECT_EQ(shared_memory_stats().total, 0u);
}

TEST_F(SharedBlockAllocatorTest, ReturnsNullWhenSharedMemoryIsExhausted) {
    SharedBlockAllocator allocator;
    fail_shared_allocations(true);
    EXPECT_EQ(allocator.allocate_stream_control(), nullptr);
    EXPECT_EQ(allocator.allocate_mirror(), nullptr);
}

TEST_F(SharedBlockAllocatorTest, ReleasesRegionsOnDestruction) {
    {
        SharedBlockAllocator allocator;
        allocator.allocate(SharedBlockAllocator::region_size, 8);
        allocator.allocate_stream_control();
    }
    EXPECT_EQ(shared_memory_stats().live, 0u);
    EXPECT_EQ(shared_memory_stats().mismatched_frees, 0u);
}

TEST_F(SharedBlockAllocatorTest, IsThreadSafe) {
    SharedBlockAllocator allocator;
    std::atomic<int> failures{0};
    const auto work = [&](int thread) {
        const auto pattern = static_cast<std::byte>(thread + 1);
        std::vector<std::pair<void*, std::uint64_t>> held;
        for (int iteration = 0; iteration < 2000; ++iteration) {
            const std::uint64_t size = 8 + static_cast<std::uint64_t>((iteration * 13) % 120);
            void* block = allocator.allocate(size, 8);
            const bool is_valid = block != nullptr && is_filled(block, size, std::byte{0});
            failures += is_valid ? 0 : 1;
            if (block == nullptr) {
                continue;
            }
            std::memset(block, static_cast<int>(pattern), size);
            held.emplace_back(block, size);
            if (held.size() > 16) {
                failures += is_filled(held.front().first, held.front().second, pattern) ? 0 : 1;
                allocator.free(held.front().first, held.front().second);
                held.erase(held.begin());
            }
        }
        for (const auto& [block, size] : held) {
            failures += is_filled(block, size, pattern) ? 0 : 1;
            allocator.free(block, size);
        }
    };
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 8; ++thread) {
        threads.emplace_back(work, thread);
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(failures.load(), 0);
}

TEST(SharedBlocks, IsOneInstance) {
    EXPECT_EQ(&shared_blocks(), &shared_blocks());
}

} // namespace
