#pragma once

#ifndef __OBJC__
#error "deferred_free.hpp is for Objective-C++ sources only"
#endif

#include "allocation.hpp"

#include <directmetal/backend/commands.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace directmetal::backend::resources {

struct PendingFree;
class CompletedFrees;

// Holds destroyed resources until every tracked queue's progress reaches the value it had last
// submitted at destruction. Timeline::notify callbacks only push onto a lock-free list, which the
// owner drains with take_completed. Not thread-safe; the arena locks.
class DeferredFrees {
public:
    DeferredFrees();
    DeferredFrees(const DeferredFrees&) = delete;
    DeferredFrees& operator=(const DeferredFrees&) = delete;
    ~DeferredFrees();

    void track(Queue& queue);
    void untrack(Queue& queue) noexcept;
    // Returns the resource when no tracked queue has outstanding work, so it can be freed now.
    std::optional<PlacedResource> defer(PlacedResource resource);
    std::vector<Allocation> take_completed();

private:
    struct Wait {
        Timeline* progress;
        std::uint64_t value;
    };

    std::vector<Wait> outstanding_waits() const;

    std::vector<Queue*> queues_;
    std::shared_ptr<CompletedFrees> completed_;
};

} // namespace directmetal::backend::resources
