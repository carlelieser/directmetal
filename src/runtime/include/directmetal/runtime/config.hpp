#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Per-application configuration. Precedence: DIRECTMETAL_CONFIG overrides, then the file's
// [app:<executable>] section, then its [*] section, then the option's default. Resolved on the
// client side, where the executable name is known; the unix side reads only environment variables.
//
// File format (UTF-8, one entry per line):
//   # comment      Lines whose first non-blank character is '#' or ';' are comments. There are
//                  no trailing comments, so values may contain '#'.
//   [*]            Options for every application.
//   [app:<name>]   Options for one application. <name> is the executable's file name without its
//                  directory, as given in Sources::application, compared ASCII case-insensitively
//                  on every platform (Windows semantics) and including any extension, so
//                  [app:Game.EXE] matches "game.exe" but not "game". Sections can repeat; later
//                  assignments win.
//   key = value    Assigns an option in the current section. Blanks around the key and value are
//                  trimmed; keys are ASCII case-insensitive. A key before any section is an error.
// Values: boolean is true/false, yes/no, on/off or 1/0 (case-insensitive); integer is a decimal
// number with an optional sign that fits in 64 bits; choice is one of the option's choices
// (case-insensitive); string is taken as written.
//
// DIRECTMETAL_CONFIG is "key=value;key=value": the same keys and values with ';' as separator, so
// values cannot contain ';'. Empty entries are ignored.
//
// Every problem is reported as a warning from log::Component::runtime and the offending entry is
// skipped, so a lower-precedence source still supplies the value: lines that are not comments,
// sections or assignments, unknown sections, unknown keys, and invalid values. Problems are
// reported for every section, not only the matching one.
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
    // A configuration with every option at its default.
    Config() noexcept = default;

    // Unknown keys and malformed lines are logged as warnings and ignored. Throws only
    // std::bad_alloc.
    static Config resolve(const Sources& sources);
    // Resolves against `registry`, which must outlive the Config; for tests.
    static Config resolve(const Sources& sources, std::span<const OptionSpec> registry);

    // An unknown key or an option of another type is a programming error: it is logged as an
    // error and the type's zero value is returned.
    bool get_bool(std::string_view key) const;
    std::int64_t get_int(std::string_view key) const;
    std::string_view get_string(std::string_view key) const;

private:
    // Nullopt, after logging an error, for an unknown key or another type.
    std::optional<std::string_view> value_of(std::string_view key, OptionType type) const;

    std::span<const OptionSpec> registry_ = options();
    // Parallel to registry_; empty means every option is at its default.
    std::vector<std::string> values_;
};

// Resolved once, thread-safely, from DIRECTMETAL_CONFIG_FILE (else <executable dir>/
// directmetal.conf; an absent default file is not an error) and DIRECTMETAL_CONFIG, for the
// running executable's file name. If memory runs out, an error is logged and defaults are used.
const Config& process_config() noexcept;

} // namespace directmetal::config
