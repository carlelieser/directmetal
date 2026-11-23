#include "calls.hpp"

#include "objects.hpp"
#include "status.hpp"
#include "stream.hpp"
#include "teardown.hpp"

#include <memory>

namespace directmetal::host {
namespace {

using namespace bridge;

bool is_valid_control(const HostContext& context, std::uint64_t address) noexcept {
    const bool is_aligned = address % alignof(StreamControl) == 0;
    return is_aligned && is_addressable(context, {address, sizeof(StreamControl)});
}

std::shared_ptr<TimelineObject> make_progress(const std::shared_ptr<Stream>& stream) {
    auto timeline = std::make_shared<TimelineObject>();
    timeline->device = stream->device();
    timeline->queue_owner = stream;
    timeline->timeline = &stream->progress();
    timeline->mirror = stream->progress_mirror();
    timeline->highest_signaled.store(0);
    return timeline;
}

// Inserts the stream and its progress timeline; on failure neither remains.
Status insert_stream(HostContext& context, const std::shared_ptr<Stream>& stream,
                     OpenStreamParams& params) {
    const Handle progress =
        context.table.insert(ObjectKind::progress_timeline, params.device, make_progress(stream));
    if (is_null(progress)) {
        return status_no_memory;
    }
    const Handle handle =
        context.table.insert(ObjectKind::stream, params.device,
                             std::make_shared<StreamObject>(StreamObject{stream, progress}));
    if (is_null(handle)) {
        context.table.remove(progress, ObjectKind::progress_timeline);
        return status_no_memory;
    }
    params.stream = handle;
    params.progress_timeline = progress;
    return status_success;
}

} // namespace

Status open_stream(HostContext& context, OpenStreamParams& params) {
    auto device = context.table.find_as<DeviceObject>(params.device, ObjectKind::device);
    if (device == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_control(context, params.control_address)) {
        return status_invalid_parameter;
    }
    const auto lock = lock_if_live(*device);
    if (!lock.owns_lock()) {
        return status_invalid_handle;
    }
    const StreamSetup setup{device, params.device,
                            from_address<StreamControl>(params.control_address), &context.table,
                            context.is_wow64.load()};
    auto opened = Stream::open(setup);
    if (!opened.has_value()) {
        return to_bridge(opened.status());
    }
    const std::shared_ptr<Stream> stream = std::move(opened).value();
    const Status status = insert_stream(context, stream, params);
    if (status < 0) {
        stream->close();
    }
    return status;
}

// Everything else about the chunk is checked on the consumer thread, where a violation is sticky.
Status submit_stream(HostContext& context, SubmitStreamParams& params) {
    const auto stream = context.table.find_as<StreamObject>(params.stream, ObjectKind::stream);
    if (stream == nullptr) {
        return status_invalid_handle;
    }
    stream->stream->submit(params);
    return status_success;
}

Status close_stream(HostContext& context, CloseStreamParams& params) {
    const TableEntry entry = context.table.find(params.stream);
    if (entry.kind != ObjectKind::stream) {
        return status_invalid_handle;
    }
    finish_streams(context, entry.owner, Unsubmitted::commit);
    retire(context, entry);
    return status_success;
}

} // namespace directmetal::host
