#pragma once

#include <mutex>
#include <string_view>

namespace directmetal::log {

// Serializes the sink, the file and stderr, so replacing the sink or the file returns only when no
// write into the old one is running.
extern std::mutex output_mutex;

// Appends `line` to the file installed by set_file. Requires output_mutex. False when there is no
// file or the write failed, in which case the caller falls back to stderr.
bool write_to_file(std::string_view line) noexcept;

} // namespace directmetal::log
