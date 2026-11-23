#pragma once

#include "command_executor.hpp"
#include "handle_table.hpp"
#include "mirror.hpp"
#include "objects.hpp"
#include "work_queue.hpp"

#include <directmetal/backend/device.hpp>
#include <directmetal/bridge/stream_decoder.hpp>

#include <cstdint>
#include <memory>
#include <thread>

namespace directmetal::host {

struct StreamSetup {
    std::shared_ptr<DeviceObject> device;
    bridge::Handle device_handle;
    // PE-side; written by the consumer thread until close() returns.
    bridge::StreamControl* control;
    HandleTable* table;
    // Chunks must then lie below 4 GiB.
    bool is_wow64;
};

// A command stream: its queue and the consumer thread that decodes submitted chunks into it. The
// thread is a plain std::thread (a pthread) and never calls into Wine.
class Stream {
public:
    static backend::Result<std::shared_ptr<Stream>> open(const StreamSetup& setup);
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    ~Stream();

    // Queues a chunk for the consumer thread.
    void submit(const bridge::SubmitStreamParams& submission);
    // Waits until every chunk queued so far is decoded, then until the GPU has completed everything
    // submitted. Recorded work no submit command has committed is submitted first when unsubmitted
    // is commit; with keep, it is left recorded and finish returns false without waiting.
    bool finish(Unsubmitted unsubmitted);
    // Stops the consumer thread; once this returns nothing writes the StreamControl.
    void close() noexcept;

    const std::shared_ptr<DeviceObject>& device() const noexcept { return device_; }
    backend::Timeline& progress() noexcept { return queue_->progress(); }
    const std::shared_ptr<MirrorWriter>& progress_mirror() const noexcept {
        return progress_mirror_;
    }

private:
    Stream(const StreamSetup& setup, std::unique_ptr<backend::Queue> queue);

    void run() noexcept;
    void decode(const bridge::SubmitStreamParams& submission) noexcept;
    void publish_failure(const bridge::DecodeFailure& failure) noexcept;
    Unsubmitted settle_mode(Unsubmitted requested) const noexcept;

    std::shared_ptr<DeviceObject> device_;
    // Destroyed before device_: a queue must not outlive its device.
    std::unique_ptr<backend::Queue> queue_;
    bridge::StreamControl* control_;
    std::shared_ptr<MirrorWriter> progress_mirror_;
    bool is_wow64_;
    CommandExecutor executor_;
    bridge::StreamDecoder decoder_;
    // A failure found before the decoder saw the chunk; sticky like the decoder's.
    bool has_failed_ = false;
    WorkQueue work_;
    std::thread thread_;
};

} // namespace directmetal::host

namespace directmetal::host {

// What a stream handle names.
struct StreamObject {
    std::shared_ptr<Stream> stream;
    bridge::Handle progress;
};

} // namespace directmetal::host
