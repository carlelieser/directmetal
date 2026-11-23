#include "unicode.hpp"

#include <array>
#include <cstdint>
#include <optional>

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

// Well-formed UTF-8 sequences by lead byte: the sequence length and the range of the second byte;
// later bytes are 0x80-0xbf. https://www.unicode.org/versions/latest/core-spec/chapter-3/#G27506
struct Utf8Lead {
    std::uint8_t first;
    std::uint8_t last;
    std::size_t length;
    std::uint8_t second_first;
    std::uint8_t second_last;
};

constexpr std::array<Utf8Lead, 9> utf8_leads = {{
    {0x00, 0x7f, 1, 0x00, 0x00},
    {0xc2, 0xdf, 2, 0x80, 0xbf},
    {0xe0, 0xe0, 3, 0xa0, 0xbf},
    {0xe1, 0xec, 3, 0x80, 0xbf},
    {0xed, 0xed, 3, 0x80, 0x9f},
    {0xee, 0xef, 3, 0x80, 0xbf},
    {0xf0, 0xf0, 4, 0x90, 0xbf},
    {0xf1, 0xf3, 4, 0x80, 0xbf},
    {0xf4, 0xf4, 4, 0x80, 0x8f},
}};

bool is_in(std::uint8_t value, std::uint8_t first, std::uint8_t last) noexcept {
    return value >= first && value <= last;
}

// The length of the well-formed sequence starting at `bytes`, if any.
std::optional<std::size_t> sequence_length(std::span<const std::byte> bytes) noexcept {
    const auto byte_at = [&](std::size_t index) {
        return std::to_integer<std::uint8_t>(bytes[index]);
    };
    for (const Utf8Lead& lead : utf8_leads) {
        if (!is_in(byte_at(0), lead.first, lead.last)) {
            continue;
        }
        if (bytes.size() < lead.length) {
            return std::nullopt;
        }
        for (std::size_t index = 1; index < lead.length; ++index) {
            const bool is_second = index == 1;
            const std::uint8_t first = is_second ? lead.second_first : std::uint8_t{0x80};
            const std::uint8_t last = is_second ? lead.second_last : std::uint8_t{0xbf};
            if (!is_in(byte_at(index), first, last)) {
                return std::nullopt;
            }
        }
        return lead.length;
    }
    return std::nullopt;
}

} // namespace

std::string utf8_sanitized(std::span<const std::byte> bytes) {
    std::string text;
    std::size_t index = 0;
    while (index < bytes.size() && bytes[index] != std::byte{0}) {
        const std::optional<std::size_t> length = sequence_length(bytes.subspan(index));
        if (length.has_value()) {
            text.append(reinterpret_cast<const char*>(bytes.data() + index), *length);
            index += *length;
        } else {
            append_utf8(text, replacement_character);
            ++index;
        }
    }
    return text;
}

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
