#include "command_executor.hpp"

#include "command_rules.hpp"
#include "status.hpp"

#include <new>
#include <utility>

namespace directmetal::host {

using bridge::DecodedCommand;
using bridge::Status;
using bridge::status_invalid_handle;
using bridge::status_invalid_parameter;
using bridge::status_success;

const std::array<CommandExecutor::Handler, static_cast<std::size_t>(bridge::Opcode::count)>
    CommandExecutor::handlers = {
        &CommandExecutor::nop,
        &CommandExecutor::set_label,
        &CommandExecutor::destroy_object,
        &CommandExecutor::clear_texture,
        &CommandExecutor::clear_depth_stencil,
        &CommandExecutor::copy_buffer,
        &CommandExecutor::copy_buffer_to_texture,
        &CommandExecutor::copy_texture_to_buffer,
        &CommandExecutor::copy_texture,
        &CommandExecutor::signal_timeline,
        &CommandExecutor::submit,
};

Status CommandExecutor::execute(const DecodedCommand& command) noexcept {
    try {
        return (this->*handlers[static_cast<std::size_t>(command.opcode())])(command);
    } catch (const std::bad_alloc&) {
        return bridge::status_no_memory;
    }
}

Settled CommandExecutor::settle(Unsubmitted unsubmitted) noexcept {
    if (has_recorded_ && unsubmitted == Unsubmitted::commit) {
        submit_recorded();
    }
    return {setup_.queue->last_submitted_value(), has_recorded_};
}

Status CommandExecutor::nop(const DecodedCommand&) {
    return status_success;
}

Status CommandExecutor::set_label(const DecodedCommand& command) {
    const bridge::Handle object = command.payload<bridge::SetLabelCommand>().object;
    if (const auto buffer = find_buffer(object); buffer != nullptr) {
        buffer->buffer->set_label(command.label());
        return status_success;
    }
    if (const auto texture = find_texture(object); texture != nullptr) {
        texture->texture->set_label(command.label());
        return status_success;
    }
    return status_invalid_handle;
}

// Imported buffers and timelines have their own release calls, so naming them is an error.
Status CommandExecutor::destroy_object(const DecodedCommand& command) {
    const bridge::Handle object = command.payload<bridge::DestroyObjectCommand>().object;
    const TableEntry entry = find(object);
    if (entry.object == nullptr) {
        return status_invalid_handle;
    }
    if (entry.kind != ObjectKind::buffer && entry.kind != ObjectKind::texture) {
        return status_invalid_parameter;
    }
    setup_.table->remove(object, entry.kind);
    return status_success;
}

Status CommandExecutor::clear_texture(const DecodedCommand& command) {
    const auto clear = command.payload<bridge::ClearTextureCommand>();
    const auto texture = find_texture(clear.texture);
    if (texture == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_color_clear(texture->desc, clear)) {
        return status_invalid_parameter;
    }
    hold(texture);
    setup_.queue->recorder().clear_texture(*texture->texture, clear.range, clear.color);
    return status_success;
}

Status CommandExecutor::clear_depth_stencil(const DecodedCommand& command) {
    const auto clear = command.payload<bridge::ClearDepthStencilCommand>();
    const auto texture = find_texture(clear.texture);
    if (texture == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_depth_clear(texture->desc, clear)) {
        return status_invalid_parameter;
    }
    hold(texture);
    setup_.queue->recorder().clear_depth_stencil(*texture->texture, clear.range, clear.clear);
    return status_success;
}

Status CommandExecutor::copy_buffer(const DecodedCommand& command) {
    const auto copy = command.payload<bridge::CopyBufferCommand>();
    const auto source = find_buffer(copy.source);
    const auto destination = find_buffer(copy.destination);
    if (source == nullptr || destination == nullptr) {
        return status_invalid_handle;
    }
    const BufferCopyCheck check{source->size, destination->size, source == destination};
    if (!is_valid_buffer_copy(copy, check)) {
        return status_invalid_parameter;
    }
    hold(source);
    hold(destination);
    setup_.queue->recorder().copy_buffer({*source->buffer, copy.source_offset, *destination->buffer,
                                          copy.destination_offset, copy.size});
    return status_success;
}

Status CommandExecutor::copy_buffer_to_texture(const DecodedCommand& command) {
    return copy_between(command.payload<bridge::CopyBufferToTextureCommand>().region, true);
}

Status CommandExecutor::copy_texture_to_buffer(const DecodedCommand& command) {
    return copy_between(command.payload<bridge::CopyTextureToBufferCommand>().region, false);
}

Status CommandExecutor::copy_between(const bridge::BufferTextureRegion& region, bool is_upload) {
    const auto buffer = find_buffer(region.buffer);
    const auto texture = find_texture(region.texture);
    if (buffer == nullptr || texture == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_buffer_texture_copy(region, buffer->size, texture->desc)) {
        return status_invalid_parameter;
    }
    hold(buffer);
    hold(texture);
    const backend::BufferTextureCopy copy{*buffer->buffer,    region.offset,     region.row_pitch,
                                          region.slice_pitch, *texture->texture, region.mip,
                                          region.slice,       region.origin,     region.extent};
    if (is_upload) {
        setup_.queue->recorder().copy_buffer_to_texture(copy);
    } else {
        setup_.queue->recorder().copy_texture_to_buffer(copy);
    }
    return status_success;
}

Status CommandExecutor::copy_texture(const DecodedCommand& command) {
    const auto copy = command.payload<bridge::CopyTextureCommand>();
    const auto source = find_texture(copy.source);
    const auto destination = find_texture(copy.destination);
    if (source == nullptr || destination == nullptr) {
        return status_invalid_handle;
    }
    const TextureRegion from{copy.source_mip, copy.source_slice, copy.source_origin, copy.extent};
    const TextureRegion to{copy.destination_mip, copy.destination_slice, copy.destination_origin,
                           copy.extent};
    const bool is_overlapping = source == destination && overlaps(from, to);
    if (is_overlapping || !is_valid_texture_copy(copy, source->desc, destination->desc)) {
        return status_invalid_parameter;
    }
    hold(source);
    hold(destination);
    setup_.queue->recorder().copy_texture({*source->texture, copy.source_mip, copy.source_slice,
                                           copy.source_origin, *destination->texture,
                                           copy.destination_mip, copy.destination_slice,
                                           copy.destination_origin, copy.extent});
    return status_success;
}

// Progress timelines are signaled only by submit, and a timeline never moves backwards.
Status CommandExecutor::signal_timeline(const DecodedCommand& command) {
    const auto signal = command.payload<bridge::SignalTimelineCommand>();
    const TableEntry entry = find(signal.timeline);
    if (entry.kind != ObjectKind::timeline) {
        return entry.kind == ObjectKind::progress_timeline ? status_invalid_parameter
                                                           : status_invalid_handle;
    }
    const auto timeline = std::static_pointer_cast<TimelineObject>(entry.object);
    if (signal.value < timeline->highest_signaled.load()) {
        return status_invalid_parameter;
    }
    hold(timeline);
    watches_.push_back({timeline, signal.value});
    timeline->highest_signaled.store(signal.value);
    setup_.queue->recorder().signal(*timeline->timeline, signal.value);
    return status_success;
}

Status CommandExecutor::submit(const DecodedCommand&) {
    return submit_recorded();
}

TableEntry CommandExecutor::find(bridge::Handle handle) const {
    TableEntry entry = setup_.table->find(handle);
    if (entry.owner.value != setup_.device.value) {
        return {handle, ObjectKind::none, {}, nullptr};
    }
    return entry;
}

std::shared_ptr<BufferObject> CommandExecutor::find_buffer(bridge::Handle handle) const {
    const TableEntry entry = find(handle);
    const bool is_buffer =
        entry.kind == ObjectKind::buffer || entry.kind == ObjectKind::imported_buffer;
    return is_buffer ? std::static_pointer_cast<BufferObject>(entry.object) : nullptr;
}

std::shared_ptr<TextureObject> CommandExecutor::find_texture(bridge::Handle handle) const {
    const TableEntry entry = find(handle);
    return entry.kind == ObjectKind::texture ? std::static_pointer_cast<TextureObject>(entry.object)
                                             : nullptr;
}

void CommandExecutor::hold(std::shared_ptr<void> object) {
    held_.push_back(std::move(object));
    has_recorded_ = true;
}

// Watches are registered only for work that was committed: a failed submit's signals never
// happen, and their callbacks would wait until the timeline is destroyed.
Status CommandExecutor::submit_recorded() noexcept {
    const Status status = to_bridge(setup_.queue->submit());
    std::vector<PendingWatch> watches;
    watches.swap(watches_);
    held_.clear();
    has_recorded_ = false;
    if (status < 0) {
        return status;
    }
    try {
        for (const PendingWatch& pending : watches) {
            watch(*pending.timeline->timeline, pending.timeline->mirror, pending.value);
        }
        watch(setup_.queue->progress(), setup_.progress_mirror,
              setup_.queue->last_submitted_value());
    } catch (const std::bad_alloc&) {
        return bridge::status_no_memory;
    }
    return status_success;
}

} // namespace directmetal::host
