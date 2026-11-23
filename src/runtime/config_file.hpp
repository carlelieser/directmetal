#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace directmetal::config {

enum class Scope { everywhere, application };

// One "key = value" read from a source. `where` names its position for diagnostics.
struct Assignment {
    Scope scope;
    std::string application;
    std::string key;
    std::string value;
    std::string where;
};

// Parses file text; malformed lines are reported and skipped.
std::vector<Assignment> parse_file_text(std::string_view text);
// Parses "key=value;key=value"; malformed entries are reported and skipped.
std::vector<Assignment> parse_overrides(std::string_view text);

} // namespace directmetal::config
