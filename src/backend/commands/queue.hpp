#pragma once

#ifndef __OBJC__
#error "queue.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include "loss_tracker.hpp"
#include "recorder.hpp"
#include "timeline.hpp"

#include <directmetal/backend/device.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

namespace directmetal::backend::commands {

// One MTLCommandQueue. Each submit commits the recorder's command buffer with a signal of the
// progress timeline. Destruction waits for the last committed command buffer, so pending frees
// released by the progress timeline never return memory the GPU still uses.
class MetalQueue final : public Queue {
public:
    MetalQueue(Device& device, id<MTLCommandQueue> queue, id<MTLSharedEvent> progress);
    MetalQueue(const MetalQueue&) = delete;
    MetalQueue& operator=(const MetalQueue&) = delete;
    ~MetalQueue() override;

    CommandRecorder& recorder() noexcept override { return recorder_; }
    Status submit() noexcept override;

    std::uint64_t last_submitted_value() const noexcept override {
        return submitted_value_.load(std::memory_order_acquire);
    }

    Timeline& progress() noexcept override { return progress_; }
    // Only test_seams.mm calls this; see testing::fail_next_submit.
    void inject_failure_for_testing() noexcept { is_failure_injected_.store(true); }

private:
    Device& device_;
    MetalTimeline progress_;
    MetalRecorder recorder_;
    std::shared_ptr<LossTracker> loss_;
    std::atomic<std::uint64_t> submitted_value_ = 0;
    std::atomic<bool> is_failure_injected_ = false;
    id<MTLCommandBuffer> last_committed_ = nil;
};

} // namespace directmetal::backend::commands
