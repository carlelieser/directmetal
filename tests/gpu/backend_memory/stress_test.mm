#include "device_fixture.hpp"
#include "fake_queue.hpp"
#include "resource_access.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

constexpr int allocation_count = 3000;
constexpr int destroyer_count = 4;
constexpr std::size_t queue_count = 2;

using ProgressValues = std::array<std::uint64_t, queue_count>;

// Heap ranges known to be in use: live, or destroyed and waiting for every queue to complete its
// release value.
struct Occupied {
    std::uintptr_t heap;
    NSUInteger begin;
    NSUInteger end;
    bool is_live;
    ProgressValues release_values;
};

bool has_reached(const ProgressValues& completed, const ProgressValues& required) {
    for (std::size_t index = 0; index < queue_count; ++index) {
        if (completed[index] < required[index]) {
            return false;
        }
    }
    return true;
}

// One thread allocates and reads memory_stats, destroyer threads destroy, and one thread per
// fake queue completes its progress. Destruction and submits share a lock so each destroyed range
// has exact release values, against which every new placement is checked.
class DeferredFreeStress {
public:
    explicit DeferredFreeStress(Device& device) : device_(device) {
        for (FakeQueue& queue : queues_) {
            resources::track_queue(device_, queue);
        }
    }
    ~DeferredFreeStress() {
        for (FakeQueue& queue : queues_) {
            resources::untrack_queue(device_, queue);
        }
    }

    void run() {
        std::vector<std::thread> signalers;
        for (FakeQueue& queue : queues_) {
            signalers.emplace_back([this, &queue] { signal_progress(queue); });
        }
        std::vector<std::thread> destroyers;
        for (int index = 0; index < destroyer_count; ++index) {
            destroyers.emplace_back([this, index] { destroy(index); });
        }
        allocate();
        is_allocating_ = false;
        for (std::thread& destroyer : destroyers) {
            destroyer.join();
        }
        is_destroying_ = false;
        for (std::thread& signaler : signalers) {
            signaler.join();
        }
    }

    int overlap_count() const noexcept { return overlaps_.load(); }
    std::uint64_t max_pending_free_bytes() const noexcept { return max_pending_; }
    std::size_t waiting_count() {
        std::size_t waiting = 0;
        for (FakeQueue& queue : queues_) {
            waiting += queue.fake_progress().waiting_count();
        }
        return waiting;
    }

private:
    void allocate() {
        std::mt19937 random(11);
        for (int token = 0; token < allocation_count; ++token) {
            const std::uint64_t size = (1 + random() % 32) * 4 * kib;
            auto buffer = make_buffer(device_, size, StorageMode::gpu_private);
            ASSERT_NE(buffer, nullptr);
            record_placement(token, *buffer);
            const MemoryStats stats = device_.memory_stats();
            ASSERT_LE(stats.used_bytes + stats.pending_free_bytes, stats.heap_bytes);
            max_pending_ = std::max(max_pending_, stats.pending_free_bytes);
            const std::lock_guard lock(handoff_mutex_);
            handoff_.emplace_back(token, std::move(buffer));
        }
    }

    void record_placement(int token, Buffer& buffer) {
        Occupied placed{};
        @autoreleasepool {
            id<MTLBuffer> metal = resources::metal_buffer(buffer);
            placed = {reinterpret_cast<std::uintptr_t>((__bridge void*)metal.heap),
                      metal.heapOffset,
                      metal.heapOffset + metal.allocatedSize,
                      true,
                      {}};
        }
        const std::lock_guard lock(ranges_mutex_);
        const ProgressValues completed =
            values([](FakeQueue& queue) { return queue.progress().completed_value(); });
        std::erase_if(occupied_, [&completed](const auto& entry) {
            return !entry.second.is_live && has_reached(completed, entry.second.release_values);
        });
        for (const auto& [other, range] : occupied_) {
            const bool is_disjoint = range.end <= placed.begin || placed.end <= range.begin;
            if (range.heap == placed.heap && !is_disjoint) {
                ++overlaps_;
            }
        }
        occupied_.emplace(token, placed);
    }

    void destroy(int seed) {
        std::mt19937 random(seed);
        while (true) {
            auto [token, buffer] = take_buffer();
            if (buffer == nullptr) {
                if (!is_allocating_) {
                    return;
                }
                std::this_thread::yield();
                continue;
            }
            const std::lock_guard submit_lock(submit_mutex_);
            for (FakeQueue& queue : queues_) {
                if (random() % 2 == 0) {
                    queue.submit();
                }
            }
            mark_released(token,
                          values([](FakeQueue& queue) { return queue.last_submitted_value(); }));
            buffer.reset();
        }
    }

    std::pair<int, std::unique_ptr<Buffer>> take_buffer() {
        const std::lock_guard lock(handoff_mutex_);
        if (handoff_.empty()) {
            return {-1, nullptr};
        }
        auto next = std::move(handoff_.front());
        handoff_.pop_front();
        return next;
    }

    template <typename Read> ProgressValues values(Read read) {
        ProgressValues read_values{};
        for (std::size_t index = 0; index < queue_count; ++index) {
            read_values[index] = read(queues_[index]);
        }
        return read_values;
    }

    void mark_released(int token, const ProgressValues& release_values) {
        const std::lock_guard lock(ranges_mutex_);
        Occupied& range = occupied_.at(token);
        range.is_live = false;
        range.release_values = release_values;
    }

    void signal_progress(FakeQueue& queue) {
        while (is_destroying_) {
            queue.fake_progress().signal_cpu(queue.last_submitted_value());
            std::this_thread::yield();
        }
        queue.fake_progress().signal_cpu(queue.last_submitted_value());
    }

    Device& device_;
    std::array<FakeQueue, queue_count> queues_;
    std::mutex handoff_mutex_;
    std::deque<std::pair<int, std::unique_ptr<Buffer>>> handoff_;
    std::mutex ranges_mutex_;
    std::map<int, Occupied> occupied_;
    std::mutex submit_mutex_;
    std::atomic<bool> is_allocating_ = true;
    std::atomic<bool> is_destroying_ = true;
    std::atomic<int> overlaps_ = 0;
    std::uint64_t max_pending_ = 0;
};

TEST(DeferredFreeStress, ConcurrentDestroyCompleteAndAllocate) {
    const auto device = make_device({.heap_block_size = 2 * mib});
    {
        DeferredFreeStress stress(*device);
        stress.run();
        EXPECT_GT(stress.max_pending_free_bytes(), 0u);
        EXPECT_EQ(stress.overlap_count(), 0);
        EXPECT_EQ(stress.waiting_count(), 0u);
    }
    const MemoryStats stats = device->memory_stats();
    EXPECT_EQ(stats.pending_free_bytes, 0u);
    EXPECT_EQ(stats.used_bytes, 0u);
}

} // namespace
