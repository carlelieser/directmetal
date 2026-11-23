#pragma once

#include <directmetal/bridge/control.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>

namespace directmetal::host {

// What a settle request does with recorded work no submit command has committed.
enum class Unsubmitted : std::uint8_t { commit, keep };

// The consumer's answer to a settle request.
struct Settled {
    // The progress value that completes everything submitted.
    std::uint64_t value;
    // Recorded work is still waiting for a submit command (only with Unsubmitted::keep).
    bool has_unsubmitted;
};

struct StreamWork {
    enum class Kind : std::uint8_t { chunk, settle, stop };

    Kind kind;
    bridge::SubmitStreamParams submission;
    // For settle: the ticket complete() reports, and what to do with unsubmitted work.
    std::uint64_t ticket;
    Unsubmitted unsubmitted;
};

// Hands submitted chunks and commit requests from control calls (the producer) to a stream's
// consumer thread, in order.
class WorkQueue {
public:
    void push_chunk(const bridge::SubmitStreamParams& submission);
    // Queues a settle request after every chunk queued so far; wait_completed(ticket) waits for
    // it. Once stop is requested, nothing is queued and the ticket is already complete.
    std::uint64_t push_settle(Unsubmitted unsubmitted);
    // pop() returns stop once everything queued before has been returned.
    void request_stop() noexcept;
    StreamWork pop() noexcept;

    // Called by the consumer once it has handled the settle request with ticket.
    void complete(std::uint64_t ticket, Settled settled) noexcept;
    Settled wait_completed(std::uint64_t ticket) noexcept;

private:
    std::mutex mutex_;
    std::condition_variable has_work_;
    std::condition_variable has_completed_;
    std::deque<StreamWork> pending_;
    bool is_stopping_ = false;
    std::uint64_t issued_ticket_ = 0;
    std::uint64_t completed_ticket_ = 0;
    Settled completed_{0, false};
};

} // namespace directmetal::host
