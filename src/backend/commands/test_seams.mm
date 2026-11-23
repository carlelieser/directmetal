#include "test_seams.hpp"

#include "queue.hpp"

namespace directmetal::backend::commands::testing {

void fail_next_submit(Queue& queue) noexcept {
    static_cast<MetalQueue&>(queue).inject_failure_for_testing();
}

void wait_on_gpu(Queue& queue, Timeline& timeline, std::uint64_t value) {
    static_cast<MetalRecorder&>(queue.recorder()).wait(timeline, value);
}

} // namespace directmetal::backend::commands::testing
