#include "teardown.hpp"

#include "objects.hpp"
#include "stream.hpp"

#include <array>
#include <vector>

namespace directmetal::host {
namespace {

void release(HostContext& context, const TableEntry& removed) noexcept {
    switch (removed.kind) {
    case ObjectKind::stream: {
        const auto* stream = static_cast<const StreamObject*>(removed.object.get());
        stream->stream->close();
        retire(context, context.table.find(stream->progress));
        break;
    }
    case ObjectKind::timeline:
    case ObjectKind::progress_timeline:
        static_cast<const TimelineObject*>(removed.object.get())->mirror->detach();
        break;
    default:
        break;
    }
}

// Streams first, so no consumer thread still uses the objects retired after them.
constexpr std::array retire_order = {ObjectKind::stream,   ObjectKind::progress_timeline,
                                     ObjectKind::timeline, ObjectKind::texture,
                                     ObjectKind::buffer,   ObjectKind::imported_buffer};

} // namespace

void finish_streams(HostContext& context, bridge::Handle device) {
    for (const TableEntry& entry : context.table.owned_by(device)) {
        if (entry.kind == ObjectKind::stream) {
            static_cast<const StreamObject*>(entry.object.get())->stream->finish();
        }
    }
}

void retire(HostContext& context, const TableEntry& entry) noexcept {
    if (entry.object == nullptr) {
        return;
    }
    const TableEntry removed = context.table.remove(entry.handle, entry.kind);
    if (removed.object != nullptr) {
        release(context, removed);
    }
}

void destroy_device_objects(HostContext& context, bridge::Handle device) {
    finish_streams(context, device);
    const std::vector<TableEntry> owned = context.table.owned_by(device);
    for (const ObjectKind kind : retire_order) {
        for (const TableEntry& entry : owned) {
            if (entry.kind == kind) {
                retire(context, entry);
            }
        }
    }
    retire(context, context.table.find(device));
}

} // namespace directmetal::host
