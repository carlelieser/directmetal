#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Operating-system services the runtime needs. Strings are UTF-8.
namespace directmetal::platform {

using FileHandle = std::intptr_t;
inline constexpr FileHandle no_file = -1;

// The value, or nullopt when the variable is unset or empty.
std::optional<std::string> environment_variable(const std::string& name);
FileHandle open_append(const std::string& path) noexcept;
bool write_all(FileHandle file, std::string_view data) noexcept;
void close_file(FileHandle file) noexcept;

// Text files larger than this are refused, so a wrong path cannot exhaust memory.
inline constexpr std::size_t max_text_file_bytes = 1 << 20;

enum class ReadStatus { ok, missing, failed };

struct ReadResult {
    ReadStatus status;
    std::string text;
};

ReadResult read_text_file(const std::string& path);
// The running executable's full path, or empty when it cannot be determined.
std::string executable_path();

} // namespace directmetal::platform
