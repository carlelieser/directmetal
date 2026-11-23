#pragma once

#include <directmetal/runtime/log.hpp>

#include <mutex>
#include <string_view>

namespace directmetal::log {

// Serializes the sink, the file and stderr, so replacing the sink or the file returns only when no
// write into the old one is running.
extern std::mutex output_mutex;

// The functions below require output_mutex.

// True when a file is installed or waiting to be opened by the first write.
bool has_log_file() noexcept;

// Appends the formatted line to the file, opening a deferred file first. False when the line was
// not written, in which case the caller falls back to stderr. The first failure after a file is
// installed is reported on stderr.
bool write_to_file(Component component, Level level, std::string_view message) noexcept;

// Names the file to open lazily at the first write, replacing any file. Used at load time, when
// opening files is best avoided (loader lock on Windows).
void defer_file(std::string_view path) noexcept;

} // namespace directmetal::log
