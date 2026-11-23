#include <directmetal/runtime/hud.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>

namespace directmetal::hud {
namespace {

constexpr std::size_t counter_count = static_cast<std::size_t>(Counter::count);

// Zero-initialized, so counting from other static initializers is safe.
constinit std::array<std::atomic<std::uint64_t>, counter_count> totals{};

// Guards the observer and the totals reported by the previous frame.
constinit std::mutex frame_mutex;
Observer* current_observer = nullptr;
std::array<std::uint64_t, counter_count> reported_totals{};

bool is_valid(Counter counter) noexcept {
    return static_cast<std::size_t>(counter) < counter_count;
}

} // namespace

void add(Counter counter, std::uint64_t amount) noexcept {
    if (is_valid(counter)) {
        totals[static_cast<std::size_t>(counter)].fetch_add(amount, std::memory_order_relaxed);
    }
}

std::uint64_t value(Counter counter) noexcept {
    if (!is_valid(counter)) {
        return 0;
    }
    return totals[static_cast<std::size_t>(counter)].load(std::memory_order_relaxed);
}

void set_observer(Observer* observer) noexcept {
    const std::lock_guard lock(frame_mutex);
    current_observer = observer;
}

void end_frame() noexcept {
    const std::lock_guard lock(frame_mutex);
    std::array<std::uint64_t, counter_count> frame{};
    for (std::size_t index = 0; index < counter_count; ++index) {
        const std::uint64_t total = totals[index].load(std::memory_order_relaxed);
        frame[index] = total - reported_totals[index];
        reported_totals[index] = total;
    }
    if (current_observer != nullptr) {
        current_observer->on_frame(frame);
    }
}

} // namespace directmetal::hud
