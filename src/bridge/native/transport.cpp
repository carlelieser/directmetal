#define DIRECTMETAL_BRIDGE_API
#include <directmetal/bridge/client.hpp>
#include <directmetal/host/dispatch.hpp>

#include <shared_allocation.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

namespace {

using namespace directmetal::bridge;

void unmap(std::byte* start, std::uint64_t size) noexcept {
    if (size == 0) {
        return;
    }
    [[maybe_unused]] const int result = munmap(start, size);
    assert(result == 0);
}

// mmap aligns to the page size, which is 4 KiB on x86_64, so the mapping is over-allocated and
// trimmed to shared_memory_alignment.
void* map_aligned(std::uint64_t size) noexcept {
    const auto page_size = static_cast<std::uint64_t>(getpagesize());
    const std::uint64_t slack =
        shared_memory_alignment > page_size ? shared_memory_alignment - page_size : 0;
    if (size > UINT64_MAX - slack) {
        return nullptr;
    }
    void* mapping =
        mmap(nullptr, size + slack, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) {
        return nullptr;
    }
    auto* start = static_cast<std::byte*>(mapping);
    const std::uint64_t misalignment =
        directmetal::bridge::to_address(start) & (shared_memory_alignment - 1);
    const std::uint64_t head = misalignment == 0 ? 0 : shared_memory_alignment - misalignment;
    unmap(start, head);
    unmap(start + head + size, slack - head);
    return start + head;
}

} // namespace

extern "C" Status directmetal_bridge_call(std::uint32_t code, void* params) noexcept {
    return directmetal::host::dispatch(code, params);
}

extern "C" void* directmetal_bridge_allocate_shared(std::uint64_t size) noexcept {
    const std::uint64_t rounded = shared_allocation_size(size);
    return rounded == 0 ? nullptr : map_aligned(rounded);
}

extern "C" void directmetal_bridge_free_shared(void* memory, std::uint64_t size) noexcept {
    const std::uint64_t rounded = shared_allocation_size(size);
    if (memory == nullptr || rounded == 0) {
        return;
    }
    unmap(static_cast<std::byte*>(memory), rounded);
}
