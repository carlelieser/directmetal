#pragma once

#include <string_view>

namespace directmetal::log {

// Appends `line` to the file installed by set_file. False when there is no file or the write
// failed, in which case the caller falls back to stderr.
bool write_to_file(std::string_view line) noexcept;

} // namespace directmetal::log
