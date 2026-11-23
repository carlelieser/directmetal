#pragma once

#include <cstdint>
#include <format>
#include <new>
#include <string_view>
#include <utility>

// Logging for every component. Lines are "directmetal: <level> <component>: <message>". Levels
// come from DIRECTMETAL_LOG once configuration is resolved; until then the default is info.
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

// Receives every enabled line instead of stderr; for tests.
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
