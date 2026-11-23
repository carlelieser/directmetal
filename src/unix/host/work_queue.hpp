#pragma once

#include <directmetal/bridge/control.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>

namespace directmetal::host {

struct StreamWork {
    enum class Kind : std::uint8_t { chunk, commit, stop };

    Kind kind;
    bridge::SubmitStreamParams submission;
    // For commit: the ticket complete() reports.
    std::uint64_t ticket;
};

// Hands submitted chunks and commit requests from control calls (the producer) to a stream's
// consumer thread, in order.
class WorkQueue {
public:
    void push_chunk(const bridge::SubmitStreamParams& submission);
    // Queues a commit after every chunk queued so far; wait_completed(ticket) waits for it. Once
    // stop is requested, nothing is queued and the ticket is already complete.
    std::uint64_t push_commit();
    // pop() returns stop once everything queued before has been returned.
    void request_stop() noexcept;
    StreamWork pop() noexcept;

    // Called by the consumer once it has handled the commit with ticket; value is the progress
    // value that completes the committed work.
    void complete(std::uint64_t ticket, std::uint64_t value) noexcept;
    std::uint64_t wait_completed(std::uint64_t ticket) noexcept;

private:
    std::mutex mutex_;
    std::condition_variable has_work_;
    std::condition_variable has_completed_;
    std::deque<StreamWork> pending_;
    bool is_stopping_ = false;
    std::uint64_t issued_ticket_ = 0;
    std::uint64_t completed_ticket_ = 0;
    std::uint64_t completed_value_ = 0;
};

} // namespace directmetal::host
