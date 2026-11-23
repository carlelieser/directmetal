#pragma once

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

} // namespace directmetal::platform
