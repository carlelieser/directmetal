#pragma once

#include <directmetal/bridge/protocol.hpp>
#include <directmetal/export.hpp>

namespace directmetal {

using UnixCallEntry = bridge::Status (*)(void* params) noexcept;

} // namespace directmetal

// Wine's unixlib interface looks these tables up by name and indexes them by bridge::FunctionCode.
extern "C" {
DIRECTMETAL_UNIX_API extern const directmetal::UnixCallEntry __wine_unix_call_funcs[];
DIRECTMETAL_UNIX_API extern const directmetal::UnixCallEntry __wine_unix_call_wow64_funcs[];
}
