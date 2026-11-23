#include "control_call.hpp"

#include <directmetal/bridge/client.hpp>
#include <directmetal/runtime/log.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace directmetal::bridge {
namespace {

constexpr std::array<std::string_view, static_cast<std::size_t>(FunctionCode::count)>
    function_names = {
        "query_version",  "log_call",        "initialize",     "enumerate_adapters",
        "create_device",  "destroy_device",  "create_buffer",  "import_memory",
        "create_texture", "create_timeline", "open_stream",    "submit_stream",
        "wait_timeline",  "close_stream",    "release_memory", "destroy_timeline",
};

constexpr bool has_every_name() noexcept {
    for (const std::string_view name : function_names) {
        if (name.empty()) {
            return false;
        }
    }
    return true;
}

// Aggregate initialization leaves names missing from the list empty.
static_assert(has_every_name(), "function_names needs an entry for every FunctionCode");

struct StatusMapping {
    Status bridge;
    backend::Status backend;
};

constexpr std::array status_mappings = {
    StatusMapping{status_success, backend::Status::success},
    StatusMapping{status_timeout, backend::Status::timeout},
    StatusMapping{status_no_memory, backend::Status::out_of_memory},
    StatusMapping{status_invalid_parameter, backend::Status::invalid_argument},
    StatusMapping{status_invalid_handle, backend::Status::invalid_argument},
    StatusMapping{status_invalid_device_state, backend::Status::invalid_argument},
    StatusMapping{status_not_implemented, backend::Status::unsupported},
    StatusMapping{status_not_supported, backend::Status::unsupported},
    StatusMapping{status_revision_mismatch, backend::Status::unsupported},
    StatusMapping{status_dll_not_found, backend::Status::unsupported},
    StatusMapping{status_entrypoint_not_found, backend::Status::unsupported},
    StatusMapping{status_device_removed, backend::Status::device_lost},
};

// Unlisted failures leave the unix side in an unknown state.
backend::Status to_backend(Status status) noexcept {
    const auto mapping =
        std::find_if(status_mappings.begin(), status_mappings.end(),
                     [status](const StatusMapping& item) { return item.bridge == status; });
    if (mapping != status_mappings.end()) {
        return mapping->backend;
    }
    return status < 0 ? backend::Status::device_lost : backend::Status::success;
}

} // namespace

backend::Status report(FunctionCode code, Status status) noexcept {
    if (status < 0) {
        log::print(log::Component::bridge, log::Level::warn, "{} failed with status {:#010x}",
                   function_names[to_index(code)], static_cast<std::uint32_t>(status));
    }
    return to_backend(status);
}

backend::Status invoke(FunctionCode code, void* params) noexcept {
    return report(code, call(code, params));
}

} // namespace directmetal::bridge
