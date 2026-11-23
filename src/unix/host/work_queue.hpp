#pragma once

#include <directmetal/bridge/control.hpp>

#include <condition_variable>
#include <cstddef>
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

// Owned by the waiting control call, so each request gets its own answer.
struct SettleRequest {
    Unsubmitted unsubmitted;
    Settled settled{0, false};
    bool is_done = false;
};

struct StreamWork {
    enum class Kind : std::uint8_t { chunk, settle, stop };

    Kind kind;
    bridge::SubmitStreamParams submission;
    // For settle; it outlives the work because its owner waits for complete().
    SettleRequest* request;
};

// Hands submitted chunks and settle requests from control calls (the producers) to a stream's
// consumer thread, in order.
class WorkQueue {
public:
    // Chunks queued and not yet taken by the consumer; push_chunk waits while this many are.
    static constexpr std::size_t max_queued_chunks = 64;

    // Blocks while max_queued_chunks are queued. Once stop is requested the chunk is dropped.
    void push_chunk(const bridge::SubmitStreamParams& submission);
    // Queues request after every chunk queued so far. False, leaving request untouched, once stop
    // has been requested: the consumer then never answers it.
    bool push_settle(SettleRequest& request);
    // pop() returns stop once everything queued before has been returned.
    void request_stop() noexcept;
    StreamWork pop() noexcept;

    // Called by the consumer once it has handled request.
    void complete(SettleRequest& request, Settled settled) noexcept;
    void wait_completed(const SettleRequest& request) noexcept;

private:
    std::mutex mutex_;
    std::condition_variable has_work_;
    std::condition_variable has_completed_;
    std::condition_variable has_space_;
    std::deque<StreamWork> pending_;
    std::size_t queued_chunks_ = 0;
    bool is_stopping_ = false;
};

} // namespace directmetal::host
