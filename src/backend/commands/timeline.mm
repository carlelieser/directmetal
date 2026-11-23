#include "timeline.hpp"

#include <algorithm>
#include <cstdint>
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

} // namespace

TimelineState::TimelineState(id<MTLSharedEvent> event) : event_(event) {}

std::uint64_t TimelineState::completed_value() const noexcept {
    return event_.signaledValue;
}

// Callers hold mutex_.
bool TimelineState::is_reached(std::uint64_t value) const {
    const bool is_pending = std::ranges::any_of(
        pending_, [value](const Pending& pending) { return pending.value <= value; });
    const bool is_running = !running_.empty() && *running_.begin() <= value;
    return event_.signaledValue >= value && !is_pending && !is_running;
}

// Callers hold mutex_.
bool TimelineState::is_failed(std::uint64_t value) const {
    const bool is_running = !running_.empty() && *running_.begin() <= value;
    return is_lost_settled_ && !is_running;
}

Status TimelineState::wait(std::uint64_t value, std::chrono::nanoseconds timeout) {
    std::unique_lock lock(mutex_);
    const auto settled = [this, value] { return is_reached(value) || is_failed(value); };
    if (!settled() && timeout > std::chrono::nanoseconds{0}) {
        lock.unlock();
        listen(value);
        lock.lock();
        if (is_unbounded(timeout)) {
            changed_.wait(lock, settled);
        } else {
            changed_.wait_for(lock, timeout, settled);
        }
    }
    if (is_reached(value)) {
        return Status::success;
    }
    return is_failed(value) ? Status::device_lost : Status::timeout;
}

void TimelineState::signal_cpu(std::uint64_t value) noexcept {
    event_.signaledValue = value;
}

void TimelineState::notify(std::uint64_t value, std::function<void()> callback) {
    std::vector<Pending> batch;
    {
        const std::lock_guard lock(mutex_);
        const bool is_due = is_lost_ || event_.signaledValue >= value;
        if (!is_due) {
            pending_.push_back({value, std::move(callback)});
        } else {
            running_.insert(value);
            batch.push_back({value, std::move(callback)});
        }
    }
    if (batch.empty()) {
        listen(value);
        return;
    }
    run(std::move(batch), false);
}

void TimelineState::release_pending(bool is_lost) {
    std::vector<Pending> batch;
    {
        const std::lock_guard lock(mutex_);
        is_lost_ = is_lost_ || is_lost;
        batch = take_pending(UINT64_MAX);
    }
    run(std::move(batch), is_lost);
}

void TimelineState::wait_idle() {
    std::unique_lock lock(mutex_);
    changed_.wait(lock, [this] { return running_.empty(); });
}

// Callers hold mutex_. Taken callbacks count as running until run() returns.
std::vector<TimelineState::Pending> TimelineState::take_pending(std::uint64_t up_to) {
    const auto due = std::ranges::stable_partition(
        pending_, [up_to](const Pending& pending) { return pending.value > up_to; });
    std::vector<Pending> batch;
    for (Pending& pending : due) {
        running_.insert(pending.value);
        batch.push_back(std::move(pending));
    }
    pending_.erase(due.begin(), due.end());
    return batch;
}

void TimelineState::run(std::vector<Pending> batch, bool is_lost) {
    for (Pending& pending : batch) {
        pending.callback();
    }
    {
        const std::lock_guard lock(mutex_);
        for (const Pending& pending : batch) {
            running_.erase(running_.find(pending.value));
        }
        is_lost_settled_ = is_lost_settled_ || is_lost;
    }
    changed_.notify_all();
}

// One listener per value, so repeated waits on the same value do not pile up listener blocks.
void TimelineState::listen(std::uint64_t value) {
    {
        const std::lock_guard lock(mutex_);
        if (!listening_.insert(value).second) {
            return;
        }
    }
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
    std::vector<Pending> batch;
    {
        const std::lock_guard lock(mutex_);
        const std::uint64_t reached = event_.signaledValue;
        batch = take_pending(reached);
        listening_.erase(listening_.begin(), listening_.upper_bound(reached));
    }
    run(std::move(batch), false);
}

MetalTimeline::MetalTimeline(id<MTLSharedEvent> event)
    : state_(std::make_shared<TimelineState>(event)) {}

// Waits for callbacks already running elsewhere, so none outlives the timeline.
MetalTimeline::~MetalTimeline() {
    state_->release_pending(false);
    state_->wait_idle();
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
