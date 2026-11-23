#pragma once

#include <directmetal/runtime/config.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace directmetal::config {

// The canonical spelling of `text` for the option's type, or nullopt when it is not a valid value:
// "true" or "false", a decimal integer, the matching choice, or the string unchanged.
std::optional<std::string> canonical_value(const OptionSpec& option, std::string_view text);

} // namespace directmetal::config
