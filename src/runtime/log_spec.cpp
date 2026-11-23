#include <directmetal/runtime/log.hpp>

#include "text.hpp"

#include <optional>

namespace directmetal::log {
namespace {

constexpr std::size_t component_count = static_cast<std::size_t>(Component::count);
constexpr std::size_t level_count = static_cast<std::size_t>(Level::off) + 1;

struct Directive {
    std::optional<Component> component;
    Level level;
};

struct ParsedEntry {
    std::optional<Directive> directive;
    std::string_view problem;
};

std::optional<Level> parse_level(std::string_view token) noexcept {
    for (std::size_t index = 0; index < level_count; ++index) {
        const auto level = static_cast<Level>(index);
        if (text::equals_ignore_case(token, name(level))) {
            return level;
        }
    }
    return std::nullopt;
}

std::optional<Component> parse_component(std::string_view token) noexcept {
    for (std::size_t index = 0; index < component_count; ++index) {
        const auto component = static_cast<Component>(index);
        if (text::equals_ignore_case(token, name(component))) {
            return component;
        }
    }
    return std::nullopt;
}

ParsedEntry parse_entry(std::string_view entry) noexcept {
    const text::Split parts = text::split_first(entry, '=');
    const std::optional<Level> level =
        parse_level(text::trim(parts.has_separator ? parts.rest : parts.head));
    if (!level) {
        return {std::nullopt, "unknown level"};
    }
    if (!parts.has_separator) {
        return {Directive{std::nullopt, *level}, {}};
    }
    const std::optional<Component> component = parse_component(text::trim(parts.head));
    if (!component) {
        return {std::nullopt, "unknown component"};
    }
    return {Directive{component, *level}, {}};
}

// Calls `visit` with each non-empty, trimmed entry.
template <typename Visitor> void for_each_entry(std::string_view spec, Visitor visit) {
    while (!spec.empty()) {
        const text::Split parts = text::split_first(spec, ',');
        const std::string_view entry = text::trim(parts.head);
        if (!entry.empty()) {
            visit(entry);
        }
        spec = parts.rest;
    }
}

void apply(const Directive& directive) noexcept {
    if (directive.component) {
        set_level(*directive.component, directive.level);
        return;
    }
    for (std::size_t index = 0; index < component_count; ++index) {
        set_level(static_cast<Component>(index), directive.level);
    }
}

} // namespace

std::size_t apply_spec(std::string_view spec) noexcept {
    // Reported before anything applies, so an "off" entry cannot hide a later typo.
    std::size_t rejected = 0;
    for_each_entry(spec, [&rejected](std::string_view entry) {
        const ParsedEntry parsed = parse_entry(entry);
        if (!parsed.directive) {
            ++rejected;
            print(Component::runtime, Level::warn, "log spec: ignoring \"{}\": {}",
                  text::excerpt(entry), parsed.problem);
        }
    });
    for_each_entry(spec, [](std::string_view entry) {
        if (const ParsedEntry parsed = parse_entry(entry); parsed.directive) {
            apply(*parsed.directive);
        }
    });
    return rejected;
}

} // namespace directmetal::log
