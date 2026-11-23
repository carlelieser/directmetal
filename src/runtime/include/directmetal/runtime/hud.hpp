#pragma once

#include <cstdint>
#include <span>

// Counters for a frame-time HUD. Adding is a relaxed atomic increment.
namespace directmetal::hud {

enum class Counter : std::uint16_t {
    stream_bytes,
    stream_flushes,
    control_calls,
    heap_bytes,
    upload_bytes,
    submits,
    count,
};

void add(Counter counter, std::uint64_t amount) noexcept;
std::uint64_t value(Counter counter) noexcept;

class Observer {
public:
    virtual ~Observer() = default;
    // counters is indexed by Counter.
    virtual void on_frame(std::span<const std::uint64_t> counters) noexcept = 0;
};

void set_observer(Observer* observer) noexcept;
// Called at Present from Phase 2 on.
void end_frame() noexcept;

} // namespace directmetal::hud
