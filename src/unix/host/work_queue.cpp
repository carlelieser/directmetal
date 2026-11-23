#include "work_queue.hpp"

namespace directmetal::host {

void WorkQueue::push_chunk(const bridge::SubmitStreamParams& submission) {
    {
        const std::lock_guard lock(mutex_);
        pending_.push_back({StreamWork::Kind::chunk, submission, nullptr});
    }
    has_work_.notify_one();
}

bool WorkQueue::push_settle(SettleRequest& request) {
    {
        const std::lock_guard lock(mutex_);
        if (is_stopping_) {
            return false;
        }
        pending_.push_back({StreamWork::Kind::settle, {}, &request});
    }
    has_work_.notify_one();
    return true;
}

void WorkQueue::request_stop() noexcept {
    {
        const std::lock_guard lock(mutex_);
        is_stopping_ = true;
    }
    has_work_.notify_one();
}

StreamWork WorkQueue::pop() noexcept {
    std::unique_lock lock(mutex_);
    has_work_.wait(lock, [this] { return !pending_.empty() || is_stopping_; });
    if (pending_.empty()) {
        return {StreamWork::Kind::stop, {}, nullptr};
    }
    const StreamWork work = pending_.front();
    pending_.pop_front();
    return work;
}

void WorkQueue::complete(SettleRequest& request, Settled settled) noexcept {
    {
        const std::lock_guard lock(mutex_);
        request.settled = settled;
        request.is_done = true;
    }
    has_completed_.notify_all();
}

void WorkQueue::wait_completed(const SettleRequest& request) noexcept {
    std::unique_lock lock(mutex_);
    has_completed_.wait(lock, [&request] { return request.is_done; });
}

} // namespace directmetal::host
