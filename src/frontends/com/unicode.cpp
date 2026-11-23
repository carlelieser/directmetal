#include "unicode.hpp"

#include <cstdint>

namespace directmetal::com {
namespace {

constexpr char32_t replacement_character = 0xfffd;

bool is_high_surrogate(char32_t unit) noexcept {
    return unit >= 0xd800 && unit <= 0xdbff;
}

bool is_low_surrogate(char32_t unit) noexcept {
    return unit >= 0xdc00 && unit <= 0xdfff;
}

void append_utf8(std::string& text, char32_t code_point) {
    const auto byte = [](char32_t bits) { return static_cast<char>(bits); };
    if (code_point < 0x80) {
        text += byte(code_point);
    } else if (code_point < 0x800) {
        text += byte(0xc0 | (code_point >> 6));
        text += byte(0x80 | (code_point & 0x3f));
    } else if (code_point < 0x10000) {
        text += byte(0xe0 | (code_point >> 12));
        text += byte(0x80 | ((code_point >> 6) & 0x3f));
        text += byte(0x80 | (code_point & 0x3f));
    } else {
        text += byte(0xf0 | (code_point >> 18));
        text += byte(0x80 | ((code_point >> 12) & 0x3f));
        text += byte(0x80 | ((code_point >> 6) & 0x3f));
        text += byte(0x80 | (code_point & 0x3f));
    }
}

} // namespace

std::string utf8_from_utf16(std::span<const std::byte> bytes) {
    const std::size_t unit_count = bytes.size() / 2;
    const auto unit_at = [&](std::size_t index) {
        return char32_t(std::to_integer<std::uint16_t>(bytes[2 * index]) |
                        (std::to_integer<std::uint16_t>(bytes[2 * index + 1]) << 8));
    };
    std::string text;
    for (std::size_t index = 0; index < unit_count; ++index) {
        const char32_t unit = unit_at(index);
        if (unit == 0) {
            break;
        }
        const bool has_next = index + 1 < unit_count;
        if (is_high_surrogate(unit) && has_next && is_low_surrogate(unit_at(index + 1))) {
            append_utf8(text, 0x10000 + ((unit - 0xd800) << 10) + (unit_at(index + 1) - 0xdc00));
            ++index;
        } else if (is_high_surrogate(unit) || is_low_surrogate(unit)) {
            append_utf8(text, replacement_character);
        } else {
            append_utf8(text, unit);
        }
    }
    return text;
}

} // namespace directmetal::com
