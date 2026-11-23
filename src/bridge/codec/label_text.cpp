#include <directmetal/bridge/command_codec.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace directmetal::bridge {
namespace {

// Well-formed multi-byte sequences by lead byte: the Unicode Standard, table 3-7.
struct LeadRule {
    std::uint8_t first_lead;
    std::uint8_t last_lead;
    std::uint8_t length;
    std::uint8_t second_min;
    std::uint8_t second_max;
};

constexpr std::array lead_rules = {
    LeadRule{0xC2, 0xDF, 2, 0x80, 0xBF}, LeadRule{0xE0, 0xE0, 3, 0xA0, 0xBF},
    LeadRule{0xE1, 0xEC, 3, 0x80, 0xBF}, LeadRule{0xED, 0xED, 3, 0x80, 0x9F},
    LeadRule{0xEE, 0xEF, 3, 0x80, 0xBF}, LeadRule{0xF0, 0xF0, 4, 0x90, 0xBF},
    LeadRule{0xF1, 0xF3, 4, 0x80, 0xBF}, LeadRule{0xF4, 0xF4, 4, 0x80, 0x8F},
};

std::uint8_t byte_at(std::string_view text, std::size_t index) noexcept {
    return static_cast<std::uint8_t>(text[index]);
}

bool is_in(std::uint8_t value, std::uint8_t first, std::uint8_t last) noexcept {
    return value >= first && value <= last;
}

// Bytes of the well-formed sequence at index, or 0 when there is none.
std::size_t sequence_length(std::string_view text, std::size_t index) noexcept {
    const std::uint8_t lead = byte_at(text, index);
    if (lead < 0x80) {
        return 1;
    }
    const auto rule =
        std::find_if(lead_rules.begin(), lead_rules.end(), [lead](const LeadRule& item) {
            return is_in(lead, item.first_lead, item.last_lead);
        });
    if (rule == lead_rules.end() || text.size() - index < rule->length) {
        return 0;
    }
    if (!is_in(byte_at(text, index + 1), rule->second_min, rule->second_max)) {
        return 0;
    }
    for (std::size_t offset = 2; offset < rule->length; ++offset) {
        if (!is_in(byte_at(text, index + offset), 0x80, 0xBF)) {
            return 0;
        }
    }
    return rule->length;
}

} // namespace

std::size_t utf8_prefix_length(std::string_view text) noexcept {
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t length = sequence_length(text, index);
        if (length == 0) {
            break;
        }
        index += length;
    }
    return index;
}

} // namespace directmetal::bridge
