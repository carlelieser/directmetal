#include <directmetal/runtime/log.hpp>

#include "log_file.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <format>
#include <new>
#include <string>
#include <utility>

namespace directmetal::log {
namespace {

constexpr std::size_t component_count = static_cast<std::size_t>(Component::count);

constexpr std::array<std::string_view, 6> level_names = {"trace", "debug", "info",
                                                         "warn",  "error", "off"};
constexpr std::array<std::string_view, component_count> component_names = {
    "runtime", "bridge", "host",  "backend", "formats", "com",
    "dxgi",    "d3d9",   "d3d10", "d3d11",   "d3d12",
};

template <std::size_t... Index>
constexpr std::array<std::atomic<Level>, sizeof...(Index)>
default_levels(std::index_sequence<Index...>) noexcept {
    return {{((void)Index, Level::info)...}};
}

// Constant-initialized, so logging from other static initializers sees the default.
constinit std::array<std::atomic<Level>, component_count> levels =
    default_levels(std::make_index_sequence<component_count>{});

constinit std::atomic<Sink*> sink{nullptr};

std::size_t index_of(Component component) noexcept {
    return static_cast<std::size_t>(component);
}

void write_to_stderr(Component component, Level level, std::string_view message) noexcept {
    const std::string_view level_name = name(level);
    const std::string_view component_name = name(component);
    std::fprintf(stderr, "directmetal: %.*s %.*s: %.*s\n", static_cast<int>(level_name.size()),
                 level_name.data(), static_cast<int>(component_name.size()), component_name.data(),
                 static_cast<int>(message.size()), message.data());
}

bool write_to_log_file(Component component, Level level, std::string_view message) noexcept {
    try {
        const std::string line =
            std::format("directmetal: {} {}: {}\n", name(level), name(component), message);
        return write_to_file(line);
    } catch (const std::bad_alloc&) {
        return false;
    }
}

} // namespace

bool is_enabled(Component component, Level level) noexcept {
    const Level threshold = levels[index_of(component)].load(std::memory_order_relaxed);
    return level != Level::off && level >= threshold;
}

void set_level(Component component, Level level) noexcept {
    levels[index_of(component)].store(level, std::memory_order_relaxed);
}

void write(Component component, Level level, std::string_view message) noexcept {
    if (Sink* current = sink.load(std::memory_order_acquire); current != nullptr) {
        current->write(component, level, message);
        return;
    }
    if (!write_to_log_file(component, level, message)) {
        write_to_stderr(component, level, message);
    }
}

void set_sink(Sink* new_sink) noexcept {
    sink.store(new_sink, std::memory_order_release);
}

std::string_view name(Level level) noexcept {
    return level_names[static_cast<std::size_t>(level)];
}

std::string_view name(Component component) noexcept {
    return component_names[index_of(component)];
}

namespace {

// Applies the environment as soon as the library loads, so later lines honor it.
struct EnvironmentInit {
    EnvironmentInit() noexcept { configure_from_environment(); }
};
const EnvironmentInit environment_init;

} // namespace

} // namespace directmetal::log
