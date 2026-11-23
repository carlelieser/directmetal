#include "work_queue.hpp"

namespace directmetal::host {

void WorkQueue::push_chunk(const bridge::SubmitStreamParams& submission) {
    {
        const std::lock_guard lock(mutex_);
        pending_.push_back({StreamWork::Kind::chunk, submission, 0, Unsubmitted::keep});
    }
    has_work_.notify_one();
}

std::uint64_t WorkQueue::push_settle(Unsubmitted unsubmitted) {
    std::uint64_t ticket = 0;
    {
        const std::lock_guard lock(mutex_);
        // The consumer may already have stopped; a ticket of 0 is complete at once.
        if (is_stopping_) {
            return 0;
        }
        ticket = issued_ticket_ + 1;
        pending_.push_back({StreamWork::Kind::settle, {}, ticket, unsubmitted});
        issued_ticket_ = ticket;
    }
    has_work_.notify_one();
    return ticket;
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
        return {StreamWork::Kind::stop, {}, 0, Unsubmitted::keep};
    }
    const StreamWork work = pending_.front();
    pending_.pop_front();
    return work;
}

void WorkQueue::complete(std::uint64_t ticket, Settled settled) noexcept {
    {
        const std::lock_guard lock(mutex_);
        completed_ticket_ = ticket;
        completed_ = settled;
    }
    has_completed_.notify_all();
}

Settled WorkQueue::wait_completed(std::uint64_t ticket) noexcept {
    if (ticket == 0) {
        return {0, false};
    }
    std::unique_lock lock(mutex_);
    has_completed_.wait(lock, [this, ticket] { return completed_ticket_ >= ticket; });
    return completed_;
}

} // namespace directmetal::host
