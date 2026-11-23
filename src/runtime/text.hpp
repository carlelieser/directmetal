#pragma once

#include <string_view>

// ASCII text helpers shared by the log specification and configuration parsers.
namespace directmetal::text {

std::string_view trim(std::string_view text) noexcept;
bool equals_ignore_case(std::string_view left, std::string_view right) noexcept;

// Splits at the first `separator`; `rest` is empty when there is none.
struct Split {
    std::string_view head;
    std::string_view rest;
    bool has_separator;
};
Split split_first(std::string_view text, char separator) noexcept;

} // namespace directmetal::text
