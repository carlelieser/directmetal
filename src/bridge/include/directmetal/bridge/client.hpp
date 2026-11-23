#pragma once

#include <directmetal/bridge/protocol.hpp>

#include <cstdint>

// Exported by directmetal.dll in PE builds and by the native bridge library in macOS builds.
//
// In Phase 1 each device has one stream, owned by one thread; that thread flushes it before any
// teardown call (destroy_device, close_stream, release_memory). Ownership of several streams per
// device is defined when deferred contexts land in Phase 4.
#ifndef DIRECTMETAL_BRIDGE_API
#ifdef _WIN32
#define DIRECTMETAL_BRIDGE_API __declspec(dllimport)
#else
#define DIRECTMETAL_BRIDGE_API
#endif
#endif

#ifdef _WIN32
#define DIRECTMETAL_CDECL __cdecl
#else
#define DIRECTMETAL_CDECL
#endif

extern "C" {

// Returns the failure that prevented connecting to the unix side when it is unavailable.
DIRECTMETAL_BRIDGE_API directmetal::bridge::Status DIRECTMETAL_CDECL
directmetal_bridge_call(std::uint32_t code, void* params) noexcept;

// Zeroed memory aligned to and rounded up to shared_memory_alignment, below 4 GiB in 32-bit
// processes; null on failure. Stream chunks use it directly. StreamControl blocks and timeline
// mirrors are carved from such regions by the client's small-block sub-allocator, so each does not
// cost a 64 KiB VirtualAlloc granule under WoW64. Imported memory may be freed only after
// release_memory returns for its buffer.
DIRECTMETAL_BRIDGE_API void* DIRECTMETAL_CDECL
directmetal_bridge_allocate_shared(std::uint64_t size) noexcept;

DIRECTMETAL_BRIDGE_API void DIRECTMETAL_CDECL
directmetal_bridge_free_shared(void* memory, std::uint64_t size) noexcept;
}

namespace directmetal::bridge {

inline Status call(FunctionCode code, void* params) noexcept {
    return directmetal_bridge_call(to_index(code), params);
}

} // namespace directmetal::bridge
