#pragma once

#include <directmetal/backend/commands.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

namespace directmetal::host {

// Keeps a PE-side std::uint64_t at the highest value published, until detached. Publishing only
// touches atomics, so it may run in Timeline::notify callbacks on a Metal completion thread.
class MirrorWriter {
public:
    // target may be null: publishing then writes nothing.
    explicit MirrorWriter(std::uint64_t* target) noexcept : target_(target) {}

    void publish(std::uint64_t value) noexcept;
    // Once this returns, publish never writes the target again.
    void detach() noexcept;

private:
    std::atomic<std::uint64_t*> target_;
    std::atomic<std::uint32_t> writers_ = 0;
};

// Publishes value to mirror once timeline completes it.
void watch(backend::Timeline& timeline, std::shared_ptr<MirrorWriter> mirror, std::uint64_t value);

} // namespace directmetal::host
