#include "test_seams.hpp"

#include "queue.hpp"

namespace directmetal::backend::commands::testing {

void fail_next_submit(Queue& queue) noexcept {
    static_cast<MetalQueue&>(queue).inject_failure_for_testing();
}

} // namespace directmetal::backend::commands::testing
