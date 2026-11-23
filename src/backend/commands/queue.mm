#include "queue.hpp"

#include "resource_access.hpp"

namespace directmetal::backend::commands {

MetalQueue::MetalQueue(Device& device, id<MTLCommandQueue> queue, id<MTLSharedEvent> progress)
    : device_(device), progress_(progress), recorder_(queue) {}

MetalQueue::~MetalQueue() {
    resources::untrack_queue(device_, *this);
    @autoreleasepool {
        [last_committed_ waitUntilCompleted];
    }
}

Status MetalQueue::submit() noexcept {
    @autoreleasepool {
        RecordedWork work = recorder_.finish();
        const std::uint64_t value = submitted_value_.load(std::memory_order_relaxed) + 1;
        [work.commands encodeSignalEvent:progress_.state()->event() value:value];
        submitted_value_.store(value, std::memory_order_release);
        [work.commands commit];
        last_committed_ = work.commands;
        return Status::success;
    }
}

} // namespace directmetal::backend::commands
