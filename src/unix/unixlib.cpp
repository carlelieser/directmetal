#include <directmetal/unixlib.hpp>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <sys/utsname.h>

namespace directmetal {
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
    std::fprintf(stderr, "directmetal: call %.*s\n", static_cast<int>(call->name_length),
                 from_address<const char>(call->name));
    return write_host_system(*call);
}

} // namespace
} // namespace directmetal

// Argument layouts are identical for 32-bit and 64-bit callers, so both tables share entries.
extern "C" {
const directmetal::UnixCallEntry __wine_unix_call_funcs[] = {
    directmetal::query_version,
    directmetal::log_call,
};
const directmetal::UnixCallEntry __wine_unix_call_wow64_funcs[] = {
    directmetal::query_version,
    directmetal::log_call,
};
}

static_assert(std::size(__wine_unix_call_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
static_assert(std::size(__wine_unix_call_wow64_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
