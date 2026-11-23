#pragma once

#include <cstdint>
#include <span>

// Counters for a frame-time HUD. Adding is a relaxed atomic increment. Counters only grow;
// value() is the total since the process started. A Counter outside the enumerators is ignored by
// add() and reads as zero.
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
    // counters is indexed by Counter and holds the amounts added since the previous end_frame.
    virtual void on_frame(std::span<const std::uint64_t> counters) noexcept = 0;
};

// Null removes the observer. When this returns, no on_frame call is running or will start with
// the old observer. The observer must outlive its installation and must not call set_observer or
// end_frame from on_frame, which would deadlock.
void set_observer(Observer* observer) noexcept;
// Called at Present from Phase 2 on. Reports the frame's counts to the observer and starts the
// next frame; calls from several threads are serialized.
void end_frame() noexcept;

} // namespace directmetal::hud
