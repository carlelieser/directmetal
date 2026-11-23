#include "log_file.hpp"

#include "platform.hpp"

#include <cstdio>
#include <format>
#include <new>
#include <string>

namespace directmetal::log {
namespace {

// All guarded by output_mutex.
platform::FileHandle current_file = platform::no_file;
constinit std::string deferred_path;
bool has_reported_failure = false;

void close_current_file() noexcept {
    if (current_file != platform::no_file) {
        platform::close_file(current_file);
    }
    current_file = platform::no_file;
}

void install(platform::FileHandle file) noexcept {
    close_current_file();
    deferred_path.clear();
    current_file = file;
    has_reported_failure = false;
}

// Opens the deferred file. Writes the failure to stderr: this runs inside the log call itself.
bool open_deferred_file() noexcept {
    const platform::FileHandle opened = platform::open_append(deferred_path);
    if (opened == platform::no_file) {
        std::fprintf(stderr, "directmetal: warn runtime: log file \"%s\": cannot open for append\n",
                     deferred_path.c_str());
        install(platform::no_file);
        return false;
    }
    install(opened);
    return true;
}

void report_write_failure() noexcept {
    if (!has_reported_failure) {
        has_reported_failure = true;
        std::fputs("directmetal: warn runtime: log file write failed; using stderr\n", stderr);
    }
}

} // namespace

bool has_log_file() noexcept {
    return current_file != platform::no_file || !deferred_path.empty();
}

bool write_to_file(Component component, Level level, std::string_view message) noexcept {
    if (current_file == platform::no_file && !open_deferred_file()) {
        return false;
    }
    try {
        const std::string line =
            std::format("directmetal: {} {}: {}\n", name(level), name(component), message);
        if (platform::write_all(current_file, line)) {
            return true;
        }
        report_write_failure();
    } catch (const std::bad_alloc&) {
        // The caller writes the line to stderr without allocating.
    }
    return false;
}

void defer_file(std::string_view path) noexcept {
    try {
        std::string copy(path);
        install(platform::no_file);
        deferred_path = std::move(copy);
    } catch (const std::bad_alloc&) {
        std::fputs("directmetal: warn runtime: log file ignored: out of memory\n", stderr);
    }
}

bool set_file(std::string_view path) noexcept {
    platform::FileHandle opened = platform::no_file;
    if (!path.empty()) {
        try {
            opened = platform::open_append(std::string(path));
        } catch (const std::bad_alloc&) {
            print(Component::runtime, Level::warn, "log file \"{}\": out of memory", path);
            return false;
        }
        if (opened == platform::no_file) {
            print(Component::runtime, Level::warn, "log file \"{}\": cannot open for append", path);
            return false;
        }
    }
    const std::lock_guard lock(output_mutex);
    install(opened);
    return true;
}

} // namespace directmetal::log
