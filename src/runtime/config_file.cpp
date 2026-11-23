#include "config_file.hpp"

#include <directmetal/runtime/log.hpp>

#include "text.hpp"

#include <optional>

namespace directmetal::config {
namespace {

using log::Component;
using log::Level;

constexpr std::string_view application_prefix = "app:";
constexpr std::string_view utf8_byte_order_mark = "\xEF\xBB\xBF";

struct Section {
    Scope scope = Scope::everywhere;
    std::string application;
    bool is_known = false;
};

bool is_comment(std::string_view line) noexcept {
    return line.front() == '#' || line.front() == ';';
}

std::optional<Section> parse_section(std::string_view line) {
    if (line.back() != ']') {
        return std::nullopt;
    }
    const std::string_view name = text::trim(line.substr(1, line.size() - 2));
    if (name == "*") {
        return Section{Scope::everywhere, {}, true};
    }
    const bool has_prefix =
        name.size() > application_prefix.size() &&
        text::equals_ignore_case(name.substr(0, application_prefix.size()), application_prefix);
    const std::string_view application = text::trim(name.substr(application_prefix.size()));
    if (!has_prefix || application.empty()) {
        return std::nullopt;
    }
    return Section{Scope::application, std::string(application), true};
}

std::optional<Assignment> parse_assignment(std::string_view entry, std::string where) {
    const text::Split parts = text::split_first(entry, '=');
    const std::string_view key = text::trim(parts.head);
    if (!parts.has_separator || key.empty()) {
        log::print(Component::runtime, Level::warn, "{}: expected key=value, got \"{}\"", where,
                   text::excerpt(entry));
        return std::nullopt;
    }
    return Assignment{Scope::everywhere,
                      {},
                      std::string(key),
                      std::string(text::trim(parts.rest)),
                      std::move(where)};
}

class FileParser {
public:
    std::vector<Assignment> parse(std::string_view text) {
        std::size_t number = 1;
        while (!text.empty()) {
            const text::Split parts = text::split_first(text, '\n');
            parse_line(text::trim(parts.head), "config file line " + std::to_string(number++));
            text = parts.rest;
        }
        return std::move(assignments_);
    }

private:
    void parse_line(std::string_view line, const std::string& where) {
        if (line.empty() || is_comment(line)) {
            return;
        }
        if (line.front() == '[') {
            begin_section(line, where);
            return;
        }
        if (!section_) {
            log::print(Component::runtime, Level::warn, "{}: \"{}\" is outside any section", where,
                       text::excerpt(line));
            return;
        }
        std::optional<Assignment> assignment = parse_assignment(line, where);
        if (assignment && section_->is_known) {
            assignment->scope = section_->scope;
            assignment->application = section_->application;
            assignments_.push_back(std::move(*assignment));
        }
    }

    // Keys of an unknown section are skipped without further warnings.
    void begin_section(std::string_view line, const std::string& where) {
        section_ = parse_section(line);
        if (!section_) {
            log::print(Component::runtime, Level::warn, "{}: unknown section \"{}\"", where,
                       text::excerpt(line));
            section_ = Section{};
        }
    }

    std::vector<Assignment> assignments_;
    std::optional<Section> section_;
};

} // namespace

std::vector<Assignment> parse_file_text(std::string_view text) {
    // Notepad writes a byte order mark that would otherwise hide the first section header.
    text.remove_prefix(text.starts_with(utf8_byte_order_mark) ? utf8_byte_order_mark.size() : 0);
    return FileParser().parse(text);
}

std::vector<Assignment> parse_overrides(std::string_view text) {
    std::vector<Assignment> assignments;
    std::size_t number = 0;
    while (!text.empty()) {
        const text::Split parts = text::split_first(text, ';');
        const std::string_view entry = text::trim(parts.head);
        text = parts.rest;
        ++number;
        if (entry.empty()) {
            continue;
        }
        const std::string where = "DIRECTMETAL_CONFIG entry " + std::to_string(number);
        if (std::optional<Assignment> assignment = parse_assignment(entry, where)) {
            assignments.push_back(std::move(*assignment));
        }
    }
    return assignments;
}

} // namespace directmetal::config
