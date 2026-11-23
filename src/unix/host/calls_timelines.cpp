#include "calls.hpp"

#include "objects.hpp"
#include "status.hpp"
#include "teardown.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>

namespace directmetal::host {
namespace {

using namespace bridge;
using std::chrono::nanoseconds;
using std::chrono::steady_clock;

// How long a wait runs before rechecking for a sticky stream error.
constexpr nanoseconds wait_slice = std::chrono::milliseconds(2);
// Longer timeouts wait without a limit, so the deadline cannot overflow.
constexpr std::uint64_t max_finite_timeout = std::uint64_t{1} << 62;

bool is_valid_mirror(const HostContext& context, std::uint64_t address) noexcept {
    if (address == 0) {
        return true;
    }
    const bool is_aligned = address % alignof(std::uint64_t) == 0;
    return is_aligned && is_addressable(context, {address, sizeof(std::uint64_t)});
}

std::shared_ptr<TimelineObject> make_timeline(std::shared_ptr<DeviceObject> device,
                                              std::unique_ptr<backend::Timeline> created,
                                              std::uint64_t mirror_address) {
    auto* mirror = from_address<std::uint64_t>(mirror_address);
    if (mirror != nullptr) {
        std::atomic_ref(*mirror).store(created->completed_value(), std::memory_order_release);
    }
    auto timeline = std::make_shared<TimelineObject>();
    timeline->device = std::move(device);
    timeline->highest_signaled.store(created->completed_value());
    timeline->timeline = created.get();
    timeline->owned = std::move(created);
    timeline->mirror = std::make_shared<MirrorWriter>(mirror);
    return timeline;
}

// Waits in slices so a stream error raised meanwhile ends the wait.
Status wait_for(const TimelineObject& timeline, std::uint64_t value, std::uint64_t timeout_ns) {
    const bool is_unlimited = timeout_ns > max_finite_timeout;
    const auto deadline = steady_clock::now() + nanoseconds(is_unlimited ? 0 : timeout_ns);
    for (;;) {
        if (timeline.device->has_failed_stream.load()) {
            return status_device_removed;
        }
        const nanoseconds remaining = is_unlimited ? wait_slice : deadline - steady_clock::now();
        const nanoseconds slice = std::clamp(remaining, nanoseconds(0), wait_slice);
        const backend::Status status = timeline.timeline->wait(value, slice);
        if (status == backend::Status::success) {
            return timeline.device->has_failed_stream.load() ? status_device_removed
                                                             : status_success;
        }
        if (status != backend::Status::timeout) {
            return to_bridge(status);
        }
        if (!is_unlimited && remaining <= slice) {
            return status_timeout;
        }
    }
}

} // namespace

Status create_timeline(HostContext& context, CreateTimelineParams& params) {
    auto device = context.table.find_as<DeviceObject>(params.device, ObjectKind::device);
    if (device == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_mirror(context, params.mirror_address)) {
        return status_invalid_parameter;
    }
    auto created = device->device->create_timeline(params.initial_value);
    if (!created.has_value()) {
        return to_bridge(created.status());
    }
    auto timeline =
        make_timeline(std::move(device), std::move(created).value(), params.mirror_address);
    const Handle handle = context.table.insert(ObjectKind::timeline, params.device, timeline);
    if (is_null(handle)) {
        timeline->mirror->detach();
        return status_no_memory;
    }
    params.timeline = handle;
    return status_success;
}

Status destroy_timeline(HostContext& context, DestroyTimelineParams& params) {
    const TableEntry entry = context.table.find(params.timeline);
    if (entry.kind != ObjectKind::timeline) {
        return status_invalid_handle;
    }
    retire(context, entry);
    return status_success;
}

Status wait_timeline(HostContext& context, WaitTimelineParams& params) {
    const TableEntry entry = context.table.find(params.timeline);
    const bool is_timeline =
        entry.kind == ObjectKind::timeline || entry.kind == ObjectKind::progress_timeline;
    if (!is_timeline) {
        return status_invalid_handle;
    }
    const auto& timeline = *static_cast<const TimelineObject*>(entry.object.get());
    return wait_for(timeline, params.value, params.timeout_ns);
}

} // namespace directmetal::host
