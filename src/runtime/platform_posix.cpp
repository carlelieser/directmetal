#include "platform.hpp"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace directmetal::platform {

std::optional<std::string> environment_variable(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

FileHandle open_append(const std::string& path) noexcept {
    const int file = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    return file < 0 ? no_file : file;
}

bool write_all(FileHandle file, std::string_view data) noexcept {
    while (!data.empty()) {
        const ssize_t written = write(static_cast<int>(file), data.data(), data.size());
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(written));
    }
    return true;
}

void close_file(FileHandle file) noexcept {
    close(static_cast<int>(file));
}

} // namespace directmetal::platform
