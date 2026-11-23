#include "calls.hpp"

#include "objects.hpp"
#include "status.hpp"
#include "teardown.hpp"
#include "texture_rules.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <span>

namespace directmetal::host {
namespace {

using namespace bridge;

std::shared_ptr<DeviceObject> find_device(const HostContext& context, Handle device) {
    return context.table.find_as<DeviceObject>(device, ObjectKind::device);
}

// Inserts entry's object under entry.kind and entry.owner and reports its handle in handle.
Status insert(HostContext& context, TableEntry entry, Handle& handle) {
    const Handle inserted = context.table.insert(entry.kind, entry.owner, std::move(entry.object));
    if (is_null(inserted)) {
        return status_no_memory;
    }
    handle = inserted;
    return status_success;
}

bool is_valid_import(const HostContext& context, const ImportMemoryParams& params) noexcept {
    const bool is_aligned =
        params.address % shared_memory_alignment == 0 && params.size % shared_memory_alignment == 0;
    if (!is_aligned || params.size == 0 || params.reserved != 0) {
        return false;
    }
    return is_addressable(context, {params.address, params.size});
}

} // namespace

Status create_buffer(HostContext& context, CreateBufferParams& params) {
    const auto device = find_device(context, params.device);
    if (device == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_buffer_desc(params.desc, device->device->capabilities())) {
        return status_invalid_parameter;
    }
    std::unique_lock lock(device->creation_mutex);
    auto created = device->device->create_buffer(params.desc);
    lock.unlock();
    if (!created.has_value()) {
        return to_bridge(created.status());
    }
    auto buffer =
        std::make_shared<BufferObject>(BufferObject{std::move(created).value(), params.desc.size});
    return insert(context, {{}, ObjectKind::buffer, params.device, std::move(buffer)},
                  params.buffer);
}

Status import_memory(HostContext& context, ImportMemoryParams& params) {
    const auto device = find_device(context, params.device);
    if (device == nullptr) {
        return status_invalid_handle;
    }
    if (!is_valid_import(context, params)) {
        return status_invalid_parameter;
    }
    const std::span memory(from_address<std::byte>(params.address), params.size);
    auto created = device->device->import_memory(memory);
    if (!created.has_value()) {
        return to_bridge(created.status());
    }
    auto buffer =
        std::make_shared<BufferObject>(BufferObject{std::move(created).value(), params.size});
    params.is_zero_copy = 1;
    return insert(context, {{}, ObjectKind::imported_buffer, params.device, std::move(buffer)},
                  params.buffer);
}

// allocation_size is the device's used_bytes growth, measured with creation serialized; it is
// only approximate while a stream destroys a resource of the same device.
Status create_texture(HostContext& context, CreateTextureParams& params) {
    const auto device = find_device(context, params.device);
    if (device == nullptr) {
        return status_invalid_handle;
    }
    backend::Device& backend_device = *device->device;
    const bool is_valid = is_valid_texture_desc(params.desc, backend_device.capabilities());
    if (!is_valid || params.reserved != 0) {
        return status_invalid_parameter;
    }
    std::unique_lock lock(device->creation_mutex);
    const std::uint64_t used_before = backend_device.memory_stats().used_bytes;
    auto created = backend_device.create_texture(params.desc);
    const std::uint64_t used_after = backend_device.memory_stats().used_bytes;
    lock.unlock();
    if (!created.has_value()) {
        return to_bridge(created.status());
    }
    auto texture =
        std::make_shared<TextureObject>(TextureObject{std::move(created).value(), params.desc});
    params.allocation_size = used_after > used_before ? used_after - used_before : 0;
    return insert(context, {{}, ObjectKind::texture, params.device, std::move(texture)},
                  params.texture);
}

Status release_memory(HostContext& context, ReleaseMemoryParams& params) {
    const TableEntry entry = context.table.find(params.buffer);
    if (entry.kind != ObjectKind::imported_buffer) {
        return status_invalid_handle;
    }
    // Committing here would signal a progress value the client never counted, and a wait on it
    // could return before the client's own submit completes.
    if (!finish_streams(context, entry.owner, Unsubmitted::keep)) {
        return status_invalid_device_state;
    }
    retire(context, entry);
    return status_success;
}

} // namespace directmetal::host
