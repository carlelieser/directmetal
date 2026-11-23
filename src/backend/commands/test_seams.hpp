#pragma once

#include <directmetal/backend/commands.hpp>

// Seams for the backend_commands GPU tests, built only into backend_commands_testing_dep and never
// into the backend itself.
namespace directmetal::backend::commands::testing {

// The next submit on queue completes as though its command buffer had failed: it does not signal
// progress, as a failed command buffer may not, and the queue then takes its device-lost path.
// Real command buffer errors are not provoked in tests. Signals the recorder encoded into that
// submit still run, so tests of a timeline's loss must not rely on them failing to.
void fail_next_submit(Queue& queue) noexcept;

} // namespace directmetal::backend::commands::testing
