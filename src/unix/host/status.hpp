#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/protocol.hpp>

namespace directmetal::host {

// The NTSTATUS a control call or stream error reports for a backend failure.
bridge::Status to_bridge(backend::Status status) noexcept;

} // namespace directmetal::host
