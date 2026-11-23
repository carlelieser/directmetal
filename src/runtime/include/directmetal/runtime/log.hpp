#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <new>
#include <string_view>
#include <utility>

// Logging for every component. Lines are "directmetal: <level> <component>: <message>". Levels
// come from DIRECTMETAL_LOG, applied when the runtime library is loaded; until then the default is
// info.
//
// DIRECTMETAL_LOG grammar, ASCII case-insensitive, whitespace around tokens ignored:
//   spec      = entry { "," entry }
//   entry     = level | component "=" level
//   level     = "trace" | "debug" | "info" | "warn" | "error" | "off"
//   component = "runtime" | "bridge" | "host" | "backend" | "formats" | "com" | "dxgi"
//             | "d3d9" | "d3d10" | "d3d11" | "d3d12"
// A bare level sets every component; entries apply left to right, so "warn,d3d11=debug" is warn
// everywhere except debug for d3d11. Empty entries are ignored. An invalid entry is reported as a
// warning and skipped; the valid entries still apply.
//
// DIRECTMETAL_LOG_FILE names a file that receives the lines instead of stderr, appended one
// write call per line. If it cannot be opened, a warning is logged and stderr stays in use.
namespace directmetal::log {

enum class Level : std::uint8_t { trace, debug, info, warn, error, off };

enum class Component : std::uint8_t {
    runtime,
    bridge,
    host,
    backend,
    formats,
    com,
    dxgi,
    d3d9,
    d3d10,
    d3d11,
    d3d12,
    count,
};

// One relaxed atomic load.
bool is_enabled(Component component, Level level) noexcept;
void set_level(Component component, Level level) noexcept;
void write(Component component, Level level, std::string_view message) noexcept;

// Applies a DIRECTMETAL_LOG specification on top of the current levels. Returns the number of
// invalid entries, each reported as a warning from Component::runtime.
std::size_t apply_spec(std::string_view spec) noexcept;

// Appends lines to the file at `path` (UTF-8) instead of stderr. An empty path restores stderr.
// Returns false, leaving the current output unchanged, if the file cannot be opened.
bool set_file(std::string_view path) noexcept;

// Applies DIRECTMETAL_LOG and DIRECTMETAL_LOG_FILE. Runs once when the library is loaded; calling
// it again re-reads the environment.
void configure_from_environment() noexcept;

// Formats only when the level is enabled.
template <typename... Args>
void print(Component component, Level level, std::format_string<Args...> format,
           Args&&... args) noexcept {
    if (!is_enabled(component, level)) {
        return;
    }
    try {
        write(component, level, std::format(format, std::forward<Args>(args)...));
    } catch (const std::bad_alloc&) {
        write(component, level, "(message dropped: out of memory)");
    }
}

// Receives every enabled line instead of stderr or the file; for tests.
class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(Component component, Level level, std::string_view message) noexcept = 0;
};

// Null restores stderr. The sink must outlive its installation.
void set_sink(Sink* sink) noexcept;

std::string_view name(Level level) noexcept;
std::string_view name(Component component) noexcept;

} // namespace directmetal::log
