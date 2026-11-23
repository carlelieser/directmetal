#pragma once

#include <cstddef>
#include <string>
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

// `text` for echoing in a diagnostic: at most max_excerpt_bytes, cut at a UTF-8 character boundary
// and followed by "..." when shortened.
inline constexpr std::size_t max_excerpt_bytes = 200;
std::string excerpt(std::string_view text);

} // namespace directmetal::text
