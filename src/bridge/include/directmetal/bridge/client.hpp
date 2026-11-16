#pragma once

#include <directmetal/bridge/protocol.hpp>

#ifndef DIRECTMETAL_BRIDGE_API
#define DIRECTMETAL_BRIDGE_API __declspec(dllimport)
#endif

// Exported by directmetal.dll. Returns the failure that prevented connecting to directmetal.so
// when the unix library is unavailable.
extern "C" DIRECTMETAL_BRIDGE_API directmetal::bridge::Status __cdecl
directmetal_bridge_call(std::uint32_t code, void* params);

namespace directmetal::bridge {

inline Status call(FunctionCode code, void* params) noexcept {
    return directmetal_bridge_call(to_index(code), params);
}

} // namespace directmetal::bridge
