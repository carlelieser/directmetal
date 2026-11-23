#pragma once

#include <directmetal/backend/types.hpp>

#include <cstddef>
#include <cstdint>

// Shared by directmetal.dll (i686, x86_64), directmetal.so (x86_64) and the native bridge. Layouts
// use fixed-width fields and carry pointers as 64-bit addresses, so 32-bit and 64-bit callers send
// identical bytes. Control-call parameters are in control.hpp and the command stream in
// commands.hpp.
//
// Shared memory: anything both sides access (stream chunks, StreamControl, timeline mirrors,
// staging memory) is allocated on the PE side with directmetal_bridge_allocate_shared. Only
// import_memory targets must be multiples of shared_memory_alignment; StreamControl needs 64-byte
// and mirrors 8-byte alignment, and the client carves them from shared regions.
namespace directmetal::bridge {

inline constexpr std::uint32_t protocol_version = 3;

// Append only; values are indexes into the unixlib call tables.
enum class FunctionCode : std::uint32_t {
    query_version,
    log_call,
    initialize,
    enumerate_adapters,
    create_device,
    destroy_device,
    create_buffer,
    import_memory,
    create_texture,
    create_timeline,
    open_stream,
    submit_stream,
    wait_timeline,
    close_stream,
    release_memory,
    destroy_timeline,
    count,
};

// NTSTATUS values.
using Status = std::int32_t;
inline constexpr Status status_success = 0;
inline constexpr Status status_timeout = 0x00000102;
inline constexpr Status status_no_more_entries = static_cast<Status>(0x8000001A);
inline constexpr Status status_unsuccessful = static_cast<Status>(0xC0000001);
inline constexpr Status status_not_implemented = static_cast<Status>(0xC0000002);
inline constexpr Status status_invalid_handle = static_cast<Status>(0xC0000008);
inline constexpr Status status_invalid_parameter = static_cast<Status>(0xC000000D);
inline constexpr Status status_no_memory = static_cast<Status>(0xC0000017);
inline constexpr Status status_revision_mismatch = static_cast<Status>(0xC0000059);
inline constexpr Status status_not_supported = static_cast<Status>(0xC00000BB);
inline constexpr Status status_dll_not_found = static_cast<Status>(0xC0000135);
inline constexpr Status status_entrypoint_not_found = static_cast<Status>(0xC0000139);
inline constexpr Status status_device_removed = static_cast<Status>(0xC00002B6);

inline constexpr std::uint32_t max_name_length = 256;
// WaitTimelineParams::timeout_ns value that never times out.
inline constexpr std::uint64_t timeout_infinite = UINT64_MAX;
// import_memory requires address and size to be multiples of this.
inline constexpr std::uint64_t shared_memory_alignment = backend::import_alignment;
inline constexpr std::uint32_t host_system_size = 16;

// An object owned by the unix side. The unix side assigns values; the PE side treats them as
// opaque. 0 is the null handle.
struct alignas(8) Handle {
    std::uint64_t value;
};

inline constexpr bool is_null(Handle handle) noexcept {
    return handle.value == 0;
}

// Callers zero every reserved field; the unix side rejects anything else with
// status_invalid_parameter so later protocol versions can assign them.

struct alignas(8) QueryVersionParams {
    std::uint32_t version;
    std::uint32_t reserved;
};

struct alignas(8) LogCallParams {
    std::uint64_t name;
    std::uint32_t name_length;
    std::uint32_t reserved;
    // Output: the unix host's uname() sysname, NUL-terminated, e.g. "Darwin".
    char host_system[host_system_size];
};

static_assert(sizeof(Handle) == 8 && alignof(Handle) == 8);
static_assert(sizeof(QueryVersionParams) == 8);
static_assert(offsetof(QueryVersionParams, version) == 0);
static_assert(offsetof(QueryVersionParams, reserved) == 4);
static_assert(sizeof(LogCallParams) == 32);
static_assert(offsetof(LogCallParams, name) == 0);
static_assert(offsetof(LogCallParams, name_length) == 8);
static_assert(offsetof(LogCallParams, reserved) == 12);
static_assert(offsetof(LogCallParams, host_system) == 16);

inline std::uint64_t to_address(const void* pointer) noexcept {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

template <typename T> inline T* from_address(std::uint64_t address) noexcept {
    return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
}

inline constexpr std::uint32_t to_index(FunctionCode code) noexcept {
    return static_cast<std::uint32_t>(code);
}

} // namespace directmetal::bridge
