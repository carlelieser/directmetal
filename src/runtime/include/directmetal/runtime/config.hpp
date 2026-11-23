#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Per-application configuration. Precedence: DIRECTMETAL_CONFIG overrides, then the file's
// [app:<executable>] section, then its [*] section, then the option's default. Resolved on the
// client side, where the executable name is known; the unix side reads only environment variables.
namespace directmetal::config {

enum class OptionType : std::uint8_t { boolean, integer, string, choice };

struct OptionSpec {
    std::string_view key;
    std::string_view default_value;
    std::string_view description;
    OptionType type;
    std::span<const std::string_view> choices;
};

// The registry of every option, as data.
std::span<const OptionSpec> options() noexcept;

struct Sources {
    std::string application;
    std::optional<std::string> file_text;
    // "key=value;key=value", from DIRECTMETAL_CONFIG.
    std::optional<std::string> overrides;
};

class Config {
public:
    // Unknown keys and malformed lines are logged as warnings and ignored.
    static Config resolve(const Sources& sources);

    bool get_bool(std::string_view key) const;
    std::int64_t get_int(std::string_view key) const;
    std::string_view get_string(std::string_view key) const;
};

// Resolved once from DIRECTMETAL_CONFIG_FILE or <executable dir>/directmetal.conf and
// DIRECTMETAL_CONFIG.
const Config& process_config() noexcept;

} // namespace directmetal::config
