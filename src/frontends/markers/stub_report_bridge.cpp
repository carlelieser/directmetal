#include "stub_report.hpp"

#include <directmetal/bridge/client.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace directmetal::frontends {

void report_stub(const char* key) noexcept {
    const std::size_t length = std::min<std::size_t>(std::strlen(key), bridge::max_name_length);
    bridge::LogCallParams params{
        bridge::to_address(key), static_cast<std::uint32_t>(length), 0, {}};
    const bridge::Status status = bridge::call(bridge::FunctionCode::log_call, &params);
    if (status < 0) {
        std::fprintf(stderr,
                     "directmetal: stub %s not logged: bridge call failed with status 0x%08x\n",
                     key, static_cast<unsigned>(status));
    }
}

} // namespace directmetal::frontends
