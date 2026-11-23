#include "queue.hpp"

#include "fault_injection.hpp"
#include "resource_access.hpp"

namespace directmetal::backend::commands {

MetalQueue::MetalQueue(Device& device, id<MTLCommandQueue> queue, id<MTLSharedEvent> progress)
    : device_(device), progress_(progress), recorder_(queue),
      loss_(std::make_shared<LossTracker>(progress_.state())) {}

MetalQueue::~MetalQueue() {
    resources::untrack_queue(device_, *this);
    @autoreleasepool {
        [last_committed_ waitUntilCompleted];
    }
}

// A refused submit discards the recorded work and releases the timelines it would signal as lost.
Status MetalQueue::submit() noexcept {
    @autoreleasepool {
        RecordedWork work = recorder_.finish();
        const std::uint64_t value = submitted_value_.load(std::memory_order_relaxed) + 1;
        if (!loss_->begin_submit(value, work.signaled)) {
            for (const auto& timeline : work.signaled) {
                timeline->release_pending(true);
            }
            return Status::device_lost;
        }
        [work.commands encodeSignalEvent:progress_.state()->event() value:value];
        const bool is_injected = is_failure_injected_.exchange(false);
        const std::shared_ptr<LossTracker> loss = loss_;
        [work.commands addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
          const bool is_error = buffer.status == MTLCommandBufferStatusError;
          loss->complete(value, is_injected || is_error);
        }];
        submitted_value_.store(value, std::memory_order_release);
        [work.commands commit];
        last_committed_ = work.commands;
        return Status::success;
    }
}

} // namespace directmetal::backend::commands

namespace directmetal::backend::commands::testing {

void fail_next_submit(Queue& queue) noexcept {
    static_cast<MetalQueue&>(queue).fail_next_submit();
}

} // namespace directmetal::backend::commands::testing
