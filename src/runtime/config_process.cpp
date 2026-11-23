#include <directmetal/runtime/config.hpp>

#include <directmetal/runtime/log.hpp>

#include "platform.hpp"

#include <new>
#include <optional>
#include <string>

namespace directmetal::config {
namespace {

using log::Component;
using log::Level;

constexpr std::string_view default_file_name = "directmetal.conf";
constexpr std::string_view path_separators = "/\\";

struct ExecutablePath {
    std::string directory;
    std::string file_name;
};

ExecutablePath split_executable_path(const std::string& path) {
    const std::size_t separator = path.find_last_of(path_separators);
    if (separator == std::string::npos) {
        return {{}, path};
    }
    return {path.substr(0, separator), path.substr(separator + 1)};
}

// An explicit file that cannot be read is reported; an absent default file is normal.
std::optional<std::string> read_config_file(const std::string& path, bool is_explicit) {
    platform::ReadResult result = platform::read_text_file(path);
    if (result.status == platform::ReadStatus::ok) {
        return std::move(result.text);
    }
    const bool is_missing = result.status == platform::ReadStatus::missing;
    if (!is_missing || is_explicit) {
        log::print(Component::runtime, Level::warn, "config file \"{}\": cannot read{}", path,
                   is_missing ? " (not found)" : "");
    }
    return std::nullopt;
}

std::optional<std::string> locate_and_read_file(const ExecutablePath& executable) {
    if (const auto path = platform::environment_variable("DIRECTMETAL_CONFIG_FILE")) {
        return read_config_file(*path, true);
    }
    if (executable.directory.empty()) {
        return std::nullopt;
    }
    return read_config_file(executable.directory + "/" + std::string(default_file_name), false);
}

Config build_process_config() noexcept {
    try {
        const ExecutablePath executable = split_executable_path(platform::executable_path());
        return Config::resolve({executable.file_name, locate_and_read_file(executable),
                                platform::environment_variable("DIRECTMETAL_CONFIG")});
    } catch (const std::bad_alloc&) {
        log::write(Component::runtime, Level::error,
                   "configuration unavailable: out of memory, using defaults");
        return Config();
    }
}

} // namespace

const Config& process_config() noexcept {
    static const Config config = build_process_config();
    return config;
}

} // namespace directmetal::config
