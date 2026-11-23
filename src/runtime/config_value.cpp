#include "config_value.hpp"

#include "text.hpp"

#include <array>
#include <charconv>
#include <cstdint>

namespace directmetal::config {
namespace {

struct BooleanSpelling {
    std::string_view text;
    bool is_true;
};

constexpr std::array<BooleanSpelling, 8> boolean_spellings = {{
    {"true", true},
    {"yes", true},
    {"on", true},
    {"1", true},
    {"false", false},
    {"no", false},
    {"off", false},
    {"0", false},
}};

std::optional<std::string> canonical_boolean(std::string_view text) {
    for (const BooleanSpelling& spelling : boolean_spellings) {
        if (text::equals_ignore_case(text, spelling.text)) {
            return spelling.is_true ? "true" : "false";
        }
    }
    return std::nullopt;
}

std::optional<std::string> canonical_integer(std::string_view text) {
    // from_chars rejects a leading '+'.
    const bool has_plus = text.size() > 1 && text[0] == '+' && text[1] != '-';
    const std::string_view digits = has_plus ? text.substr(1) : text;
    std::int64_t number = 0;
    const char* const end = digits.data() + digits.size();
    const auto [parsed_end, error] = std::from_chars(digits.data(), end, number);
    if (error != std::errc{} || parsed_end != end) {
        return std::nullopt;
    }
    return std::to_string(number);
}

std::optional<std::string> canonical_choice(const OptionSpec& option, std::string_view text) {
    for (const std::string_view choice : option.choices) {
        if (text::equals_ignore_case(text, choice)) {
            return std::string(choice);
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> canonical_value(const OptionSpec& option, std::string_view text) {
    switch (option.type) {
    case OptionType::boolean:
        return canonical_boolean(text);
    case OptionType::integer:
        return canonical_integer(text);
    case OptionType::choice:
        return canonical_choice(option, text);
    case OptionType::string:
        return std::string(text);
    }
    return std::nullopt;
}

} // namespace directmetal::config
