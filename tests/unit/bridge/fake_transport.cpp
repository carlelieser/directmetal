#define DIRECTMETAL_BRIDGE_API
#include "fake_transport.hpp"

#include <directmetal/bridge/client.hpp>

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

namespace directmetal::bridge::test {
namespace {

struct FakeTransport {
    std::mutex mutex;
    CallHandler handler;
    std::map<void*, std::uint64_t> allocations;
    std::size_t total = 0;
    std::size_t mismatched_frees = 0;
    bool is_failing = false;
};

FakeTransport& transport() {
    static FakeTransport instance;
    return instance;
}

std::uint64_t rounded(std::uint64_t size) {
    return (size + shared_memory_alignment - 1) / shared_memory_alignment * shared_memory_alignment;
}

} // namespace

void set_call_handler(CallHandler handler) {
    const std::lock_guard lock(transport().mutex);
    transport().handler = std::move(handler);
}

SharedMemoryStats shared_memory_stats() {
    const std::lock_guard lock(transport().mutex);
    return {transport().allocations.size(), transport().total, transport().mismatched_frees};
}

void reset_shared_memory_stats() {
    const std::lock_guard lock(transport().mutex);
    transport().total = 0;
    transport().mismatched_frees = 0;
}

void fail_shared_allocations(bool is_failing) {
    const std::lock_guard lock(transport().mutex);
    transport().is_failing = is_failing;
}

} // namespace directmetal::bridge::test

using namespace directmetal::bridge;
using directmetal::bridge::test::CallHandler;
using directmetal::bridge::test::rounded;
using directmetal::bridge::test::transport;

extern "C" Status directmetal_bridge_call(std::uint32_t code, void* params) noexcept {
    CallHandler handler;
    {
        const std::lock_guard lock(transport().mutex);
        handler = transport().handler;
    }
    return handler ? handler(static_cast<FunctionCode>(code), params) : status_not_implemented;
}

extern "C" void* directmetal_bridge_allocate_shared(std::uint64_t size) noexcept {
    const std::lock_guard lock(transport().mutex);
    if (transport().is_failing || size == 0) {
        return nullptr;
    }
    void* memory = std::aligned_alloc(shared_memory_alignment, rounded(size));
    std::memset(memory, 0, rounded(size));
    transport().allocations[memory] = rounded(size);
    ++transport().total;
    return memory;
}

extern "C" void directmetal_bridge_free_shared(void* memory, std::uint64_t size) noexcept {
    const std::lock_guard lock(transport().mutex);
    const auto allocation = transport().allocations.find(memory);
    if (allocation == transport().allocations.end() || allocation->second != rounded(size)) {
        ++transport().mismatched_frees;
        return;
    }
    transport().allocations.erase(allocation);
    std::free(memory);
}
