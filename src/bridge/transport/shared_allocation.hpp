#pragma once

#include <directmetal/bridge/protocol.hpp>

#include <cstdint>

// Size rule shared by the PE and native implementations of directmetal_bridge_allocate_shared.
namespace directmetal::bridge {

// size rounded up to shared_memory_alignment; 0 when size is 0 or rounding overflows.
constexpr std::uint64_t shared_allocation_size(std::uint64_t size) noexcept {
    constexpr std::uint64_t mask = shared_memory_alignment - 1;
    const bool is_representable = size != 0 && size <= UINT64_MAX - mask;
    return is_representable ? (size + mask) & ~mask : 0;
}

static_assert((shared_memory_alignment & (shared_memory_alignment - 1)) == 0);
static_assert(shared_allocation_size(1) == shared_memory_alignment);
static_assert(shared_allocation_size(shared_memory_alignment) == shared_memory_alignment);
static_assert(shared_allocation_size(UINT64_MAX) == 0);

} // namespace directmetal::bridge
