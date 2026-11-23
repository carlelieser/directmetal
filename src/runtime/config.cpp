#include <directmetal/runtime/config.hpp>

#include <directmetal/runtime/log.hpp>

#include "config_file.hpp"
#include "config_value.hpp"
#include "text.hpp"

#include <array>
#include <charconv>
#include <optional>

namespace directmetal::config {
namespace {

using log::Component;
using log::Level;

// An assignment checked against the registry.
struct Setting {
    Scope scope;
    std::string application;
    std::size_t option_index;
    std::string value;
};

constexpr std::string_view type_name(OptionType type) noexcept {
    constexpr std::array<std::string_view, 4> names = {"boolean", "integer", "string", "choice"};
    return names[static_cast<std::size_t>(type)];
}

std::optional<std::size_t> find_option(std::span<const OptionSpec> registry,
                                       std::string_view key) noexcept {
    for (std::size_t index = 0; index < registry.size(); ++index) {
        if (text::equals_ignore_case(registry[index].key, key)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<Setting> validate(const Assignment& assignment,
                                std::span<const OptionSpec> registry) {
    const std::optional<std::size_t> index = find_option(registry, assignment.key);
    if (!index) {
        log::print(Component::runtime, Level::warn, "{}: unknown option \"{}\"", assignment.where,
                   text::excerpt(assignment.key));
        return std::nullopt;
    }
    const OptionSpec& option = registry[*index];
    std::optional<std::string> value = canonical_value(option, assignment.value);
    if (!value) {
        log::print(Component::runtime, Level::warn, "{}: invalid {} value \"{}\" for option \"{}\"",
                   assignment.where, type_name(option.type), text::excerpt(assignment.value),
                   option.key);
        return std::nullopt;
    }
    return Setting{assignment.scope, assignment.application, *index, std::move(*value)};
}

std::vector<Setting> validate_all(const std::vector<Assignment>& assignments,
                                  std::span<const OptionSpec> registry) {
    std::vector<Setting> settings;
    for (const Assignment& assignment : assignments) {
        if (std::optional<Setting> setting = validate(assignment, registry)) {
            settings.push_back(std::move(*setting));
        }
    }
    return settings;
}

void apply_everywhere(const std::vector<Setting>& settings, std::vector<std::string>& values) {
    for (const Setting& setting : settings) {
        if (setting.scope == Scope::everywhere) {
            values[setting.option_index] = setting.value;
        }
    }
}

void apply_application(const std::vector<Setting>& settings, std::string_view application,
                       std::vector<std::string>& values) {
    for (const Setting& setting : settings) {
        const bool is_matching = text::equals_ignore_case(setting.application, application);
        if (setting.scope == Scope::application && is_matching) {
            values[setting.option_index] = setting.value;
        }
    }
}

std::vector<std::string> defaults(std::span<const OptionSpec> registry) {
    std::vector<std::string> values;
    for (const OptionSpec& option : registry) {
        values.emplace_back(option.default_value);
    }
    return values;
}

} // namespace

Config Config::resolve(const Sources& sources) {
    return resolve(sources, options());
}

Config Config::resolve(const Sources& sources, std::span<const OptionSpec> registry) {
    const std::vector<Setting> file = validate_all(
        sources.file_text ? parse_file_text(*sources.file_text) : std::vector<Assignment>{},
        registry);
    const std::vector<Setting> overrides = validate_all(
        sources.overrides ? parse_overrides(*sources.overrides) : std::vector<Assignment>{},
        registry);
    Config config;
    config.registry_ = registry;
    config.values_ = defaults(registry);
    apply_everywhere(file, config.values_);
    apply_application(file, sources.application, config.values_);
    apply_everywhere(overrides, config.values_);
    return config;
}

std::optional<std::string_view> Config::value_of(std::string_view key, OptionType type) const {
    const std::optional<std::size_t> index = find_option(registry_, key);
    // A choice is read as a string.
    const bool is_compatible =
        index && (registry_[*index].type == type ||
                  (type == OptionType::string && registry_[*index].type == OptionType::choice));
    if (!is_compatible) {
        log::print(Component::runtime, Level::error, "config: no {} option \"{}\"", type_name(type),
                   text::excerpt(key));
        return std::nullopt;
    }
    return values_.empty() ? registry_[*index].default_value : std::string_view(values_[*index]);
}

bool Config::get_bool(std::string_view key) const {
    return value_of(key, OptionType::boolean).value_or("") == "true";
}

std::int64_t Config::get_int(std::string_view key) const {
    const std::optional<std::string_view> found = value_of(key, OptionType::integer);
    if (!found) {
        return 0;
    }
    const std::string_view value = *found;
    std::int64_t number = 0;
    const char* const end = value.data() + value.size();
    const auto [parsed_end, error] = std::from_chars(value.data(), end, number);
    if (error != std::errc{} || parsed_end != end) {
        log::print(Component::runtime, Level::error,
                   "config: option \"{}\" holds \"{}\", not an integer", text::excerpt(key),
                   text::excerpt(value));
        return 0;
    }
    return number;
}

std::string_view Config::get_string(std::string_view key) const {
    return value_of(key, OptionType::string).value_or("");
}

} // namespace directmetal::config
