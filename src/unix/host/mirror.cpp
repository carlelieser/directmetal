#include "mirror.hpp"

#include <thread>
#include <utility>

namespace directmetal::host {

void MirrorWriter::publish(std::uint64_t value) noexcept {
    // Counted before the target is read, so detach() sees every write that could still happen.
    writers_.fetch_add(1);
    if (std::uint64_t* target = target_.load(); target != nullptr) {
        std::atomic_ref<std::uint64_t> mirror(*target);
        std::uint64_t current = mirror.load(std::memory_order_relaxed);
        while (current < value &&
               !mirror.compare_exchange_weak(current, value, std::memory_order_release)) {
        }
    }
    writers_.fetch_sub(1);
}

void MirrorWriter::detach() noexcept {
    target_.store(nullptr);
    while (writers_.load() != 0) {
        std::this_thread::yield();
    }
}

void watch(backend::Timeline& timeline, std::shared_ptr<MirrorWriter> mirror, std::uint64_t value) {
    timeline.notify(value, [mirror = std::move(mirror), value] { mirror->publish(value); });
}

} // namespace directmetal::host
