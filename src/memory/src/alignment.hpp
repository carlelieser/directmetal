#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace directmetal::memory {

constexpr bool is_power_of_two(std::uint64_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

// Rounds value up to a multiple of alignment, a power of two; nullopt when the result overflows.
constexpr std::optional<std::uint64_t> align_up(std::uint64_t value,
                                                std::uint64_t alignment) noexcept {
    const std::uint64_t mask = alignment - 1;
    if (value > std::numeric_limits<std::uint64_t>::max() - mask) {
        return std::nullopt;
    }
    return (value + mask) & ~mask;
}

} // namespace directmetal::memory
