#include "call_log_platform.hpp"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace directmetal::frontends::call_log_platform {

std::optional<std::string> log_directory() {
    const char* directory = std::getenv("DIRECTMETAL_CALL_LOG_DIR");
    if (directory == nullptr || *directory == '\0') {
        return std::nullopt;
    }
    return std::string(directory);
}

std::string executable_name() {
    return getprogname();
}

std::uint32_t process_id() noexcept {
    return static_cast<std::uint32_t>(getpid());
}

bool fill_random(std::span<std::byte> buffer) noexcept {
    arc4random_buf(buffer.data(), buffer.size());
    return true;
}

FileHandle open_append(const std::string& directory, const std::string& file_name) {
    const std::string path = directory + "/" + file_name;
    const int file = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    return file < 0 ? no_file : file;
}

bool write_line(FileHandle file, std::string_view line) noexcept {
    const ssize_t written = write(static_cast<int>(file), line.data(), line.size());
    return written == static_cast<ssize_t>(line.size());
}

unsigned long last_error() noexcept {
    return static_cast<unsigned long>(errno);
}

} // namespace directmetal::frontends::call_log_platform
