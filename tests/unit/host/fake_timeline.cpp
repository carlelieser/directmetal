#include "fake_backend.hpp"

#include <algorithm>
#include <thread>

namespace directmetal::host::testing {

bool DeferredCallbacks::defer(std::function<void()>& callback) {
    const std::lock_guard lock(mutex_);
    if (!is_deferring) {
        return false;
    }
    callbacks_.push_back(std::move(callback));
    return true;
}

void DeferredCallbacks::run_all() {
    std::vector<std::function<void()>> callbacks;
    {
        const std::lock_guard lock(mutex_);
        callbacks.swap(callbacks_);
    }
    for (auto& callback : callbacks) {
        callback();
    }
}

// A queue completing work may still be signaling this timeline through a token it locked;
// waiting for it stands in for the command buffer that keeps a Metal event alive.
FakeTimeline::~FakeTimeline() {
    const std::weak_ptr<int> in_use = token_;
    token_.reset();
    while (!in_use.expired()) {
        std::this_thread::yield();
    }
    for (auto& waiter : waiting_) {
        waiter.second();
    }
}

std::uint64_t FakeTimeline::completed_value() const noexcept {
    const std::lock_guard lock(mutex_);
    return completed_;
}

backend::Status FakeTimeline::wait(std::uint64_t value, std::chrono::nanoseconds timeout) noexcept {
    std::unique_lock lock(mutex_);
    const auto is_reached = [this, value] { return completed_ >= value; };
    if (timeout == std::chrono::nanoseconds::max()) {
        has_advanced_.wait(lock, is_reached);
        return backend::Status::success;
    }
    return has_advanced_.wait_for(lock, timeout, is_reached) ? backend::Status::success
                                                             : backend::Status::timeout;
}

void FakeTimeline::signal_cpu(std::uint64_t value) noexcept {
    std::vector<std::function<void()>> ready;
    {
        const std::lock_guard lock(mutex_);
        completed_ = std::max(completed_, value);
        std::erase_if(waiting_, [&](auto& waiter) {
            if (waiter.first > completed_) {
                return false;
            }
            ready.push_back(std::move(waiter.second));
            return true;
        });
    }
    has_advanced_.notify_all();
    for (auto& callback : ready) {
        const bool is_deferred = hooks_.deferred != nullptr && hooks_.deferred->defer(callback);
        if (!is_deferred) {
            callback();
        }
    }
}

void FakeTimeline::notify(std::uint64_t value, std::function<void()> callback) {
    if (hooks_.notify_count != nullptr) {
        ++*hooks_.notify_count;
    }
    {
        const std::lock_guard lock(mutex_);
        if (completed_ < value) {
            waiting_.emplace_back(value, std::move(callback));
            return;
        }
    }
    callback();
}

} // namespace directmetal::host::testing
