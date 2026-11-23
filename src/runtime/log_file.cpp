#include "log_file.hpp"

#include <directmetal/runtime/log.hpp>

#include "platform.hpp"

#include <mutex>
#include <new>
#include <string>

namespace directmetal::log {
namespace {

// Guarded by output_mutex.
platform::FileHandle current_file = platform::no_file;

} // namespace

bool write_to_file(std::string_view line) noexcept {
    return current_file != platform::no_file && platform::write_all(current_file, line);
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
    if (current_file != platform::no_file) {
        platform::close_file(current_file);
    }
    current_file = opened;
    return true;
}

} // namespace directmetal::log
