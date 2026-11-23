#include "stream.hpp"

#include "status.hpp"

#include <atomic>
#include <chrono>
#include <system_error>

namespace directmetal::host {
namespace {

using bridge::StreamControl;

constexpr std::uint64_t wow64_address_limit = std::uint64_t{1} << 32;

bool is_below_wow64_limit(const bridge::SubmitStreamParams& submission) noexcept {
    const bool is_start_below = submission.chunk_address < wow64_address_limit;
    return is_start_below &&
           submission.chunk_size <= wow64_address_limit - submission.chunk_address;
}

void reset_control(StreamControl& control) noexcept {
    std::atomic_ref(control.retired_sequence).store(0, std::memory_order_relaxed);
    std::atomic_ref(control.failed_opcode).store(0, std::memory_order_relaxed);
    std::atomic_ref(control.failed_sequence).store(0, std::memory_order_relaxed);
    std::atomic_ref(control.completed_progress).store(0, std::memory_order_relaxed);
    std::atomic_ref(control.status).store(bridge::status_success, std::memory_order_release);
}

} // namespace

backend::Result<std::shared_ptr<Stream>> Stream::open(const StreamSetup& setup) {
    auto queue = setup.device->device->create_queue();
    if (!queue.has_value()) {
        return backend::Result<std::shared_ptr<Stream>>::failure(queue.status());
    }
    std::shared_ptr<Stream> stream(new Stream(setup, std::move(queue).value()));
    try {
        stream->thread_ = std::thread([raw = stream.get()] { raw->run(); });
    } catch (const std::system_error&) {
        return backend::Result<std::shared_ptr<Stream>>::failure(backend::Status::out_of_memory);
    }
    return backend::Result<std::shared_ptr<Stream>>::success(std::move(stream));
}

Stream::Stream(const StreamSetup& setup, std::unique_ptr<backend::Queue> queue)
    : device_(setup.device), queue_(std::move(queue)), control_(setup.control),
      progress_mirror_(std::make_shared<MirrorWriter>(&setup.control->completed_progress)),
      is_wow64_(setup.is_wow64),
      executor_({setup.table, setup.device_handle, queue_.get(), progress_mirror_}) {
    reset_control(*control_);
}

Stream::~Stream() {
    close();
}

void Stream::submit(const bridge::SubmitStreamParams& submission) {
    work_.push_chunk(submission);
}

bool Stream::finish(Unsubmitted unsubmitted) {
    const Settled settled = work_.wait_completed(work_.push_settle(unsubmitted));
    if (settled.has_unsubmitted) {
        return false;
    }
    queue_->progress().wait(settled.value, std::chrono::nanoseconds::max());
    return true;
}

void Stream::close() noexcept {
    if (thread_.joinable()) {
        work_.request_stop();
        thread_.join();
    }
    progress_mirror_->detach();
}

void Stream::run() noexcept {
    for (;;) {
        const StreamWork work = work_.pop();
        switch (work.kind) {
        case StreamWork::Kind::chunk:
            decode(work.submission);
            break;
        case StreamWork::Kind::settle:
            work_.complete(work.ticket, executor_.settle(settle_mode(work.unsubmitted)));
            break;
        case StreamWork::Kind::stop:
            return;
        }
    }
}

// A failed stream ignores every later chunk, so no submit command can commit what it recorded
// before failing; its waits already fail, so committing it shifts no numbering anyone observes.
Unsubmitted Stream::settle_mode(Unsubmitted requested) const noexcept {
    const bool is_failed = has_failed_ || decoder_.has_failed();
    return is_failed ? Unsubmitted::commit : requested;
}

void Stream::decode(const bridge::SubmitStreamParams& submission) noexcept {
    if (has_failed_ || decoder_.has_failed()) {
        return;
    }
    if (is_wow64_ && !is_below_wow64_limit(submission)) {
        has_failed_ = true;
        publish_failure({bridge::status_invalid_parameter, bridge::framing_failure_opcode,
                         submission.sequence});
        return;
    }
    if (decoder_.decode(submission, executor_) < 0) {
        publish_failure(decoder_.failure());
        return;
    }
    std::atomic_ref(control_->retired_sequence)
        .store(submission.sequence, std::memory_order_release);
}

// Waits on the device's timelines fail before the PE side can observe the status.
void Stream::publish_failure(const bridge::DecodeFailure& failure) noexcept {
    device_->has_failed_stream.store(true);
    std::atomic_ref(control_->failed_opcode).store(failure.opcode, std::memory_order_relaxed);
    std::atomic_ref(control_->failed_sequence).store(failure.sequence, std::memory_order_relaxed);
    std::atomic_ref(control_->status).store(failure.status, std::memory_order_release);
}

} // namespace directmetal::host
