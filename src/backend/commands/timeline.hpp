#pragma once

#ifndef __OBJC__
#error "timeline.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include <directmetal/backend/commands.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace directmetal::backend::commands {

// The shared state behind a MetalTimeline. Metal listener blocks hold it weakly, so a listener
// that fires after the timeline is gone does nothing. A wait returns only once no notify callback
// at or below the awaited value is pending or still running.
class TimelineState : public std::enable_shared_from_this<TimelineState> {
public:
    explicit TimelineState(id<MTLSharedEvent> event);
    TimelineState(const TimelineState&) = delete;
    TimelineState& operator=(const TimelineState&) = delete;

    id<MTLSharedEvent> event() const noexcept { return event_; }
    std::uint64_t completed_value() const noexcept;
    Status wait(std::uint64_t value, std::chrono::nanoseconds timeout);
    void signal_cpu(std::uint64_t value) noexcept;
    void notify(std::uint64_t value, std::function<void()> callback);
    // Runs every pending callback and wakes every waiter. A lost timeline fails later waits for
    // values it has not reached and runs later callbacks immediately.
    void release_pending(bool is_lost);
    // Blocks until no callback is running.
    void wait_idle();

private:
    struct Pending {
        std::uint64_t value;
        std::function<void()> callback;
    };

    bool is_reached(std::uint64_t value) const;
    bool is_failed(std::uint64_t value) const;
    std::vector<Pending> take_pending(std::uint64_t up_to);
    void run(std::vector<Pending> batch, bool is_lost);
    void listen(std::uint64_t value);
    void deliver();

    id<MTLSharedEvent> event_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<Pending> pending_;
    // Values of callbacks taken from pending_ that have not returned yet.
    std::multiset<std::uint64_t> running_;
    // Values with a registered listener that has not fired yet.
    std::set<std::uint64_t> listening_;
    // Set when loss is reported, so later callbacks run at once; is_lost_settled_ is set once the
    // callbacks released by the loss have returned, and only then do waits fail.
    bool is_lost_ = false;
    bool is_lost_settled_ = false;
};

class MetalTimeline final : public Timeline {
public:
    explicit MetalTimeline(id<MTLSharedEvent> event);
    ~MetalTimeline() override;

    std::uint64_t completed_value() const noexcept override;
    Status wait(std::uint64_t value, std::chrono::nanoseconds timeout) noexcept override;
    void signal_cpu(std::uint64_t value) noexcept override;
    void notify(std::uint64_t value, std::function<void()> callback) override;
    const std::shared_ptr<TimelineState>& state() const noexcept { return state_; }

private:
    std::shared_ptr<TimelineState> state_;
};

// Every Timeline the backend hands out is a MetalTimeline.
const std::shared_ptr<TimelineState>& timeline_state(Timeline& timeline) noexcept;

} // namespace directmetal::backend::commands
