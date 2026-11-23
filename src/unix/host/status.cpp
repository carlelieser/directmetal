#include "status.hpp"

#include <array>
#include <cstddef>

namespace directmetal::host {
namespace {

// Indexed by backend::Status.
constexpr std::array<bridge::Status, 6> bridge_statuses = {
    bridge::status_success,       bridge::status_no_memory,      bridge::status_invalid_parameter,
    bridge::status_not_supported, bridge::status_device_removed, bridge::status_timeout,
};

} // namespace

bridge::Status to_bridge(backend::Status status) noexcept {
    const auto index = static_cast<std::size_t>(status);
    return index < bridge_statuses.size() ? bridge_statuses[index] : bridge::status_unsuccessful;
}

} // namespace directmetal::host
