#include <directmetal/runtime/log.hpp>

#include "platform.hpp"

#include <new>
#include <optional>
#include <string>

namespace directmetal::log {

void configure_from_environment() noexcept {
    try {
        if (const auto spec = platform::environment_variable("DIRECTMETAL_LOG")) {
            apply_spec(*spec);
        }
        if (const auto path = platform::environment_variable("DIRECTMETAL_LOG_FILE")) {
            set_file(*path);
        }
    } catch (const std::bad_alloc&) {
        write(Component::runtime, Level::error, "environment ignored: out of memory");
    }
}

} // namespace directmetal::log
