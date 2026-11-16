#include <directmetal/bridge/client.hpp>
#include <directmetal/frontends/markers.hpp>

#include <algorithm>
#include <cstring>

namespace directmetal::frontends {

bool report_stub(const char* key) noexcept {
    const std::size_t length = std::min<std::size_t>(std::strlen(key), bridge::max_name_length);
    bridge::LogCallParams params{bridge::to_address(key), static_cast<std::uint32_t>(length), 0};
    return bridge::call(bridge::FunctionCode::log_call, &params) >= 0;
}

} // namespace directmetal::frontends
