#include <directmetal/host/dispatch.hpp>

#include <cstdio>
#include <cstring>
#include <sys/utsname.h>

// Interim dispatcher: answers query_version and log_call and reports every other control call as
// not implemented until the host's handle table and streams land.
namespace directmetal::host {
namespace {

using namespace bridge;

Status query_version(void* params) noexcept {
    auto* query = static_cast<QueryVersionParams*>(params);
    if (query == nullptr || query->reserved != 0) {
        return status_invalid_parameter;
    }
    query->version = protocol_version;
    return status_success;
}

Status write_host_system(LogCallParams& call) noexcept {
    utsname host{};
    if (uname(&host) != 0) {
        return status_unsuccessful;
    }
    std::strncpy(call.host_system, host.sysname, sizeof(call.host_system) - 1);
    call.host_system[sizeof(call.host_system) - 1] = '\0';
    return status_success;
}

Status log_call(void* params) noexcept {
    auto* call = static_cast<LogCallParams*>(params);
    if (call == nullptr) {
        return status_invalid_parameter;
    }
    const bool has_valid_name = call->name != 0 && call->name_length <= max_name_length;
    if (!has_valid_name || call->reserved != 0) {
        return status_invalid_parameter;
    }
    // The reply is written first so a failure never follows a printed line: the PE side would
    // otherwise report a logged call as not logged.
    const Status status = write_host_system(*call);
    if (status < 0) {
        return status;
    }
    std::fprintf(stderr, "directmetal: call %.*s\n", static_cast<int>(call->name_length),
                 from_address<const char>(call->name));
    return status_success;
}

} // namespace

bridge::Status dispatch(std::uint32_t code, void* params) noexcept {
    switch (static_cast<FunctionCode>(code)) {
    case FunctionCode::query_version:
        return query_version(params);
    case FunctionCode::log_call:
        return log_call(params);
    case FunctionCode::count:
        return status_invalid_parameter;
    default:
        return code < to_index(FunctionCode::count) ? status_not_implemented
                                                    : status_invalid_parameter;
    }
}

} // namespace directmetal::host
