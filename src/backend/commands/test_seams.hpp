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

// Commands recorded on queue after this wait on the GPU until timeline reaches value. The test
// must signal timeline before destroying the queue, which waits for its submitted work.
void wait_on_gpu(Queue& queue, Timeline& timeline, std::uint64_t value);

} // namespace directmetal::backend::commands::testing
