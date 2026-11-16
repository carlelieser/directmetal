#pragma once

#include <cstddef>
#include <cstdint>

// Shared by directmetal.dll (i686, x86_64) and directmetal.so (x86_64). Layouts use fixed-width
// fields and carry pointers as 64-bit addresses, so 32-bit and 64-bit callers send identical bytes.
namespace directmetal::bridge {

inline constexpr std::uint32_t protocol_version = 1;

enum class FunctionCode : std::uint32_t {
    query_version,
    log_call,
    count,
};

// NTSTATUS values.
using Status = std::int32_t;
inline constexpr Status status_success = 0;
inline constexpr Status status_invalid_parameter = static_cast<Status>(0xC000000D);
inline constexpr Status status_revision_mismatch = static_cast<Status>(0xC0000059);
inline constexpr Status status_dll_not_found = static_cast<Status>(0xC0000135);
inline constexpr Status status_entrypoint_not_found = static_cast<Status>(0xC0000139);

inline constexpr std::uint32_t max_name_length = 256;

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
};

static_assert(sizeof(QueryVersionParams) == 8);
static_assert(offsetof(QueryVersionParams, version) == 0);
static_assert(offsetof(QueryVersionParams, reserved) == 4);
static_assert(sizeof(LogCallParams) == 16);
static_assert(offsetof(LogCallParams, name) == 0);
static_assert(offsetof(LogCallParams, name_length) == 8);
static_assert(offsetof(LogCallParams, reserved) == 12);

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
