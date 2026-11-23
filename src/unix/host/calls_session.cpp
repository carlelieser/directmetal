#include "calls.hpp"

#include "objects.hpp"
#include "status.hpp"
#include "teardown.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <sys/utsname.h>

namespace directmetal::host {
namespace {

using namespace bridge;

// runtime::log::Level::off; levels above it are unknown.
constexpr std::uint32_t max_log_level = 5;

Status write_host_system(LogCallParams& params) noexcept {
    utsname host{};
    if (uname(&host) != 0) {
        return status_unsuccessful;
    }
    std::strncpy(params.host_system, host.sysname, sizeof(params.host_system) - 1);
    params.host_system[sizeof(params.host_system) - 1] = '\0';
    return status_success;
}

bool is_valid_initialize(const InitializeParams& params) noexcept {
    const bool has_known_fields =
        (params.flags & ~initialize_flag_wow64) == 0 && params.reserved == 0;
    const bool is_valid_length = params.client_name_length <= max_name_length;
    if (!has_known_fields || !is_valid_length || params.log_level > max_log_level) {
        return false;
    }
    const bool is_wow64 = (params.flags & initialize_flag_wow64) != 0;
    return params.client_name_length == 0 ||
           is_addressable({params.client_name, params.client_name_length}, is_wow64);
}

// 0 keeps the backend's default.
std::uint64_t or_default(std::uint64_t value, std::uint64_t fallback) noexcept {
    return value == 0 ? fallback : value;
}

} // namespace

Status query_version(HostContext&, QueryVersionParams& params) {
    if (params.reserved != 0) {
        return status_invalid_parameter;
    }
    params.version = protocol_version;
    return status_success;
}

// Allowed before initialize: Phase 0 stubs log their calls without initializing.
Status log_call(HostContext& context, LogCallParams& params) {
    const bool has_valid_name = params.name_length <= max_name_length &&
                                is_addressable(context, {params.name, params.name_length});
    if (!has_valid_name || params.reserved != 0) {
        return status_invalid_parameter;
    }
    // The reply is written first so a failure never follows a printed line: the PE side would
    // otherwise report a logged call as not logged.
    const Status status = write_host_system(params);
    if (status < 0) {
        return status;
    }
    std::fprintf(stderr, "directmetal: call %.*s\n", static_cast<int>(params.name_length),
                 from_address<const char>(params.name));
    return status_success;
}

// A process is either WoW64 or not, so a repeat must agree with the first call.
Status initialize(HostContext& context, InitializeParams& params) {
    if (!is_valid_initialize(params)) {
        return status_invalid_parameter;
    }
    const bool is_wow64 = (params.flags & initialize_flag_wow64) != 0;
    const std::lock_guard lock(context.session_mutex);
    if (context.is_initialized.load() && context.is_wow64.load() != is_wow64) {
        return status_invalid_parameter;
    }
    context.is_wow64.store(is_wow64);
    context.is_initialized.store(true);
    return status_success;
}

Status enumerate_adapters(HostContext& context, EnumerateAdaptersParams& params) {
    if (params.reserved != 0) {
        return status_invalid_parameter;
    }
    const std::lock_guard lock(context.session_mutex);
    if (!context.adapters.has_value()) {
        context.adapters = context.provider.adapters();
    }
    if (params.index >= context.adapters->size()) {
        return status_no_more_entries;
    }
    params.adapter = (*context.adapters)[params.index];
    return status_success;
}

Status create_device(HostContext& context, CreateDeviceParams& params) {
    const DeviceOptions& options = params.options;
    if (options.heap_block_size % shared_memory_alignment != 0) {
        return status_invalid_parameter;
    }
    const backend::DeviceRequest defaults{};
    const backend::DeviceRequest request{
        params.registry_id, or_default(options.heap_block_size, defaults.heap_block_size),
        or_default(options.dedicated_threshold, defaults.dedicated_threshold)};
    auto created = context.provider.create(request);
    if (!created.has_value()) {
        return to_bridge(created.status());
    }
    auto device = std::make_shared<DeviceObject>(std::move(created).value());
    const backend::Capabilities capabilities = device->device->capabilities();
    const Handle handle = context.table.insert(ObjectKind::device, {}, std::move(device));
    if (is_null(handle)) {
        return status_no_memory;
    }
    params.device = handle;
    params.capabilities = capabilities;
    return status_success;
}

Status destroy_device(HostContext& context, DestroyDeviceParams& params) {
    if (context.table.find_as<DeviceObject>(params.device, ObjectKind::device) == nullptr) {
        return status_invalid_handle;
    }
    destroy_device_objects(context, params.device);
    return status_success;
}

} // namespace directmetal::host
