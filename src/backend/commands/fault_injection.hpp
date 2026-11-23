#pragma once

#include <directmetal/backend/commands.hpp>

// Test seam: real command buffer errors cannot be provoked safely, so tests mark a submit failed.
namespace directmetal::backend::commands::testing {

// The next submit on queue completes as though its command buffer had failed with an error.
void fail_next_submit(Queue& queue) noexcept;

} // namespace directmetal::backend::commands::testing
