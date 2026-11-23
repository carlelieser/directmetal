#include <directmetal/runtime/config.hpp>

#include <array>

namespace directmetal::config {
namespace {

constexpr std::array<OptionSpec, 1> registry = {{
    {"hud", "false", "Show the frame-time HUD.", OptionType::boolean, {}},
}};

} // namespace

std::span<const OptionSpec> options() noexcept {
    return registry;
}

} // namespace directmetal::config
