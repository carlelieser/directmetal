#pragma once

#ifndef __OBJC__
#error "loss_tracker.hpp is for Objective-C++ sources only"
#endif

#include "timeline.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace directmetal::backend::commands {

// A queue's device-lost state, shared with its command buffers' completion handlers. After a
// command buffer fails, the queue refuses further submits; once its last committed command buffer
// completes, its progress timeline and every timeline it has signaled are released as lost, so
// waiters return Status::device_lost and pending notify callbacks run. Thread-safe.
class LossTracker {
public:
    explicit LossTracker(const std::shared_ptr<TimelineState>& progress);

    // Records a submit about to commit value. False, recording nothing, once the queue is lost.
    bool begin_submit(std::uint64_t value,
                      const std::vector<std::shared_ptr<TimelineState>>& signaled);
    // Called by the completion handler of the command buffer that signals value.
    void complete(std::uint64_t value, bool is_failed);

private:
    void remember(const std::shared_ptr<TimelineState>& timeline);

    std::mutex mutex_;
    bool is_lost_ = false;
    std::uint64_t committed_value_ = 0;
    std::vector<std::weak_ptr<TimelineState>> signaled_;
};

} // namespace directmetal::backend::commands
