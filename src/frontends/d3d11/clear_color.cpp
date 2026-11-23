#include "clear_color.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace directmetal::d3d11 {
namespace {

struct Range {
    double lowest;
    double highest;
};

Range range_of(backend::ClearKind kind, std::uint8_t bits) noexcept {
    const double span = std::ldexp(1.0, bits);
    if (kind == backend::ClearKind::sint_value) {
        return {-span / 2, span / 2 - 1};
    }
    return {0, span - 1};
}

std::uint32_t integer_bits(FLOAT value, backend::ClearKind kind, std::uint8_t bits) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    const Range range = range_of(kind, bits);
    const double clamped =
        std::clamp(std::trunc(static_cast<double>(value)), range.lowest, range.highest);
    // Two's complement for signed channels, as the 32-bit clear value stores them.
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(clamped));
}

backend::ClearKind clear_kind_of(formats::ComponentKind kind) noexcept {
    if (kind == formats::ComponentKind::uint) {
        return backend::ClearKind::uint_value;
    }
    if (kind == formats::ComponentKind::sint) {
        return backend::ClearKind::sint_value;
    }
    return backend::ClearKind::float_value;
}

} // namespace

backend::ClearColor to_clear_color(const FLOAT color[4],
                                   const formats::FormatInfo& format) noexcept {
    backend::ClearColor clear{};
    clear.kind = clear_kind_of(format.kind);
    for (std::size_t channel = 0; channel < clear.bits.size(); ++channel) {
        const bool is_float = clear.kind == backend::ClearKind::float_value;
        clear.bits[channel] = is_float ? std::bit_cast<std::uint32_t>(color[channel])
                                       : integer_bits(color[channel], clear.kind, 32);
    }
    return clear;
}

} // namespace directmetal::d3d11
