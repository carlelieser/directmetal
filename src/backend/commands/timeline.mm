#include "timeline.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace directmetal::backend::commands {
namespace {

// One serial dispatch queue delivers every timeline's notifications.
MTLSharedEventListener* shared_listener() {
    static MTLSharedEventListener* const listener = [[MTLSharedEventListener alloc]
        initWithDispatchQueue:dispatch_queue_create("directmetal.timeline", DISPATCH_QUEUE_SERIAL)];
    return listener;
}

// Timeouts too long to add to the current time wait without a limit, as timeout_infinite does.
bool is_unbounded(std::chrono::nanoseconds timeout) {
    using Clock = std::chrono::steady_clock;
    return timeout >= Clock::time_point::max() - Clock::now();
}

void run_all(std::vector<std::function<void()>>& callbacks) {
    for (auto& callback : callbacks) {
        callback();
    }
}

} // namespace

TimelineState::TimelineState(id<MTLSharedEvent> event)
    : event_(event), delivered_value_(event.signaledValue) {}

std::uint64_t TimelineState::completed_value() const noexcept {
    return event_.signaledValue;
}

bool TimelineState::is_settled(std::uint64_t value) const noexcept {
    return is_lost_ || delivered_value_ >= value;
}

// A value Metal has reached counts as delivered once no callback at or below it is still pending.
Status TimelineState::wait(std::uint64_t value, std::chrono::nanoseconds timeout) {
    std::unique_lock lock(mutex_);
    const bool has_pending_callback = std::ranges::any_of(
        pending_, [value](const Pending& pending) { return pending.value <= value; });
    if (event_.signaledValue >= value && !has_pending_callback) {
        return Status::success;
    }
    if (!is_settled(value)) {
        lock.unlock();
        listen(value);
        lock.lock();
    }
    const auto settled = [this, value] { return is_settled(value); };
    if (is_unbounded(timeout)) {
        changed_.wait(lock, settled);
    } else if (!changed_.wait_for(lock, std::max(timeout, std::chrono::nanoseconds{0}), settled)) {
        return Status::timeout;
    }
    return delivered_value_ >= value ? Status::success : Status::device_lost;
}

void TimelineState::signal_cpu(std::uint64_t value) noexcept {
    event_.signaledValue = value;
}

void TimelineState::notify(std::uint64_t value, std::function<void()> callback) {
    bool is_due = false;
    {
        const std::lock_guard lock(mutex_);
        is_due = is_lost_ || event_.signaledValue >= value;
        if (!is_due) {
            pending_.push_back({value, std::move(callback)});
        }
    }
    if (is_due) {
        callback();
        return;
    }
    listen(value);
}

void TimelineState::release_pending(bool is_lost) {
    std::vector<std::function<void()>> callbacks;
    {
        const std::lock_guard lock(mutex_);
        is_lost_ = is_lost_ || is_lost;
        for (Pending& pending : pending_) {
            callbacks.push_back(std::move(pending.callback));
        }
        pending_.clear();
    }
    run_all(callbacks);
    changed_.notify_all();
}

void TimelineState::listen(std::uint64_t value) {
    @autoreleasepool {
        std::weak_ptr<TimelineState> weak_state = weak_from_this();
        [event_ notifyListener:shared_listener()
                       atValue:value
                         block:^(id<MTLSharedEvent>, std::uint64_t) {
                           if (const auto state = weak_state.lock()) {
                               state->deliver();
                           }
                         }];
    }
}

// Runs on the listener's queue, so deliveries never overlap.
void TimelineState::deliver() {
    std::vector<std::function<void()>> callbacks;
    std::uint64_t reached = 0;
    {
        const std::lock_guard lock(mutex_);
        reached = event_.signaledValue;
        const auto due = std::ranges::stable_partition(
            pending_, [reached](const Pending& pending) { return pending.value > reached; });
        std::ranges::transform(due, std::back_inserter(callbacks),
                               [](Pending& pending) { return std::move(pending.callback); });
        pending_.erase(due.begin(), due.end());
    }
    run_all(callbacks);
    {
        const std::lock_guard lock(mutex_);
        delivered_value_ = std::max(delivered_value_, reached);
    }
    changed_.notify_all();
}

MetalTimeline::MetalTimeline(id<MTLSharedEvent> event)
    : state_(std::make_shared<TimelineState>(event)) {}

MetalTimeline::~MetalTimeline() {
    state_->release_pending(false);
}

std::uint64_t MetalTimeline::completed_value() const noexcept {
    return state_->completed_value();
}

Status MetalTimeline::wait(std::uint64_t value, std::chrono::nanoseconds timeout) noexcept {
    return state_->wait(value, timeout);
}

void MetalTimeline::signal_cpu(std::uint64_t value) noexcept {
    state_->signal_cpu(value);
}

void MetalTimeline::notify(std::uint64_t value, std::function<void()> callback) {
    state_->notify(value, std::move(callback));
}

const std::shared_ptr<TimelineState>& timeline_state(Timeline& timeline) noexcept {
    return static_cast<MetalTimeline&>(timeline).state();
}

} // namespace directmetal::backend::commands
