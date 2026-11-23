#include "text.hpp"

#include <algorithm>

namespace directmetal::text {
namespace {

constexpr std::string_view whitespace = " \t\r\n";

char fold(char character) noexcept {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                : character;
}

} // namespace

std::string_view trim(std::string_view text) noexcept {
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

bool equals_ignore_case(std::string_view left, std::string_view right) noexcept {
    return std::ranges::equal(left, right,
                              [](char lhs, char rhs) { return fold(lhs) == fold(rhs); });
}

Split split_first(std::string_view text, char separator) noexcept {
    const std::size_t position = text.find(separator);
    if (position == std::string_view::npos) {
        return {text, {}, false};
    }
    return {text.substr(0, position), text.substr(position + 1), true};
}

} // namespace directmetal::text
