#include "loss_tracker.hpp"

#include <algorithm>

namespace directmetal::backend::commands {

LossTracker::LossTracker(const std::shared_ptr<TimelineState>& progress) : signaled_{progress} {}

void LossTracker::remember(const std::shared_ptr<TimelineState>& timeline) {
    const bool is_known = std::ranges::any_of(
        signaled_, [&timeline](const auto& known) { return known.lock() == timeline; });
    if (!is_known) {
        signaled_.push_back(timeline);
    }
}

bool LossTracker::begin_submit(std::uint64_t value,
                               const std::vector<std::shared_ptr<TimelineState>>& signaled) {
    const std::lock_guard lock(mutex_);
    if (is_lost_) {
        return false;
    }
    std::erase_if(signaled_, [](const auto& known) { return known.expired(); });
    for (const auto& timeline : signaled) {
        remember(timeline);
    }
    committed_value_ = value;
    return true;
}

void LossTracker::complete(std::uint64_t value, bool is_failed) {
    std::vector<std::shared_ptr<TimelineState>> released;
    {
        const std::lock_guard lock(mutex_);
        is_lost_ = is_lost_ || is_failed;
        const bool is_drained = value == committed_value_;
        if (!is_lost_ || !is_drained) {
            return;
        }
        for (const auto& known : signaled_) {
            if (auto timeline = known.lock()) {
                released.push_back(std::move(timeline));
            }
        }
    }
    for (const auto& timeline : released) {
        timeline->release_pending(true);
    }
}

} // namespace directmetal::backend::commands
