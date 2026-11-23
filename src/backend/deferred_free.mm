#include "deferred_free.hpp"

#include <algorithm>

namespace directmetal::backend::resources {

struct PendingFree {
    PlacedResource resource;
    std::atomic<std::size_t> remaining;
    PendingFree* next = nullptr;
};

// A Treiber stack: push is lock-free and take_all swaps the whole list out, so it has no ABA.
// Shared with the notify callbacks, which may outlive the arena if a timeline completes late.
class CompletedFrees {
public:
    CompletedFrees() = default;
    CompletedFrees(const CompletedFrees&) = delete;
    CompletedFrees& operator=(const CompletedFrees&) = delete;

    ~CompletedFrees() {
        PendingFree* node = take_all();
        while (node != nullptr) {
            delete std::exchange(node, node->next);
        }
    }

    void push(PendingFree* node) noexcept {
        node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(node->next, node, std::memory_order_release,
                                            std::memory_order_relaxed)) {
        }
    }

    PendingFree* take_all() noexcept { return head_.exchange(nullptr, std::memory_order_acquire); }

private:
    std::atomic<PendingFree*> head_ = nullptr;
};

namespace {

void complete_one(const std::shared_ptr<CompletedFrees>& completed, PendingFree* pending) {
    if (pending->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        completed->push(pending);
    }
}

} // namespace

DeferredFrees::DeferredFrees() : completed_(std::make_shared<CompletedFrees>()) {}

DeferredFrees::~DeferredFrees() = default;

void DeferredFrees::track(Queue& queue) {
    queues_.push_back(&queue);
}

void DeferredFrees::untrack(Queue& queue) noexcept {
    std::erase(queues_, &queue);
}

std::vector<DeferredFrees::Wait> DeferredFrees::outstanding_waits() const {
    std::vector<Wait> waits;
    for (Queue* queue : queues_) {
        const std::uint64_t submitted = queue->last_submitted_value();
        Timeline& progress = queue->progress();
        if (progress.completed_value() < submitted) {
            waits.push_back({&progress, submitted});
        }
    }
    return waits;
}

// remaining starts one above the wait count so callbacks that run inside notify cannot complete
// the record before every notify is registered.
std::optional<PlacedResource> DeferredFrees::defer(PlacedResource resource) {
    const std::vector<Wait> waits = outstanding_waits();
    if (waits.empty()) {
        return resource;
    }
    auto* pending = new PendingFree{std::move(resource), waits.size() + 1};
    for (const Wait& wait : waits) {
        wait.progress->notify(
            wait.value, [completed = completed_, pending] { complete_one(completed, pending); });
    }
    complete_one(completed_, pending);
    return std::nullopt;
}

std::vector<Allocation> DeferredFrees::take_completed() {
    std::vector<Allocation> allocations;
    PendingFree* node = completed_->take_all();
    while (node != nullptr) {
        allocations.push_back(node->resource.allocation);
        delete std::exchange(node, node->next);
    }
    return allocations;
}

} // namespace directmetal::backend::resources
