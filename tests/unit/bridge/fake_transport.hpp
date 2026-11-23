#pragma once

#include <directmetal/bridge/protocol.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>

// Stands in for directmetal.dll in client tests: control calls go to the installed handler and
// shared memory comes from the heap, with every allocation tracked.
namespace directmetal::bridge::test {

using CallHandler = std::function<Status(FunctionCode code, void* params)>;

// An empty handler answers every call with status_not_implemented.
void set_call_handler(CallHandler handler);

struct SharedMemoryStats {
    std::size_t live;
    std::size_t total;
    // Frees whose size did not match the allocation, or of memory never allocated.
    std::size_t mismatched_frees;
};

SharedMemoryStats shared_memory_stats();
void reset_shared_memory_stats();
// While set, directmetal_bridge_allocate_shared returns null.
void fail_shared_allocations(bool is_failing);

} // namespace directmetal::bridge::test
