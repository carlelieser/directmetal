#include <directmetal/unixlib.hpp>

#include <cstdio>
#include <iterator>

namespace directmetal {
namespace {

using namespace bridge;

Status query_version(void* params) {
    auto* query = static_cast<QueryVersionParams*>(params);
    if (query == nullptr) {
        return status_invalid_parameter;
    }
    query->version = protocol_version;
    return status_success;
}

Status log_call(void* params) {
    const auto* call = static_cast<const LogCallParams*>(params);
    const bool has_name = call != nullptr && call->name != 0;
    if (!has_name || call->name_length > max_name_length) {
        return status_invalid_parameter;
    }
    std::fprintf(stderr, "directmetal: call %.*s\n", static_cast<int>(call->name_length),
                 from_address<const char>(call->name));
    return status_success;
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
