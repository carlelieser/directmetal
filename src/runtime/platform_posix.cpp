#include "platform.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <mach-o/dyld.h>
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

ReadResult read_text_file(const std::string& path) {
    const int file = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (file < 0) {
        return {errno == ENOENT || errno == ENOTDIR ? ReadStatus::missing : ReadStatus::failed, {}};
    }
    ReadResult result{ReadStatus::ok, {}};
    std::array<char, 4096> buffer;
    while (true) {
        const ssize_t count = read(file, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) {
            continue;
        }
        const bool is_too_large =
            count > 0 && result.text.size() + static_cast<std::size_t>(count) > max_text_file_bytes;
        if (count < 0 || is_too_large) {
            result = {ReadStatus::failed, {}};
            break;
        }
        if (count == 0) {
            break;
        }
        result.text.append(buffer.data(), static_cast<std::size_t>(count));
    }
    close(file);
    return result;
}

std::string executable_path() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        return {};
    }
    path.resize(path.find('\0'));
    return path;
}

} // namespace directmetal::platform
