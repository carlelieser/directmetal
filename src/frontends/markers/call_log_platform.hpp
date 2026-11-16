#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Operating-system services the call log needs. Strings are UTF-8.
namespace directmetal::frontends::call_log_platform {

using FileHandle = std::intptr_t;
inline constexpr FileHandle no_file = -1;

std::optional<std::string> log_directory();
std::string executable_name();
std::uint32_t process_id() noexcept;
bool fill_random(std::span<std::byte> buffer) noexcept;
FileHandle open_append(const std::string& directory, const std::string& file_name);
// Writes `line` with one write call.
bool write_line(FileHandle file, std::string_view line) noexcept;
unsigned long last_error() noexcept;

} // namespace directmetal::frontends::call_log_platform
