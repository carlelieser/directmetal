#pragma once

#include "handle_table.hpp"

#include <directmetal/host/dispatch.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace directmetal::host {

// The state every control call shares.
struct HostContext {
    explicit HostContext(DeviceProvider& device_provider) noexcept : provider(device_provider) {}

    DeviceProvider& provider;
    HandleTable table;
    std::atomic<bool> is_initialized = false;
    std::atomic<bool> is_wow64 = false;
    // Guards initialize and adapters.
    std::mutex session_mutex;
    std::optional<std::vector<backend::Capabilities>> adapters;
};

struct AddressRange {
    std::uint64_t address;
    std::uint64_t size;
};

// Whether range starts at a nonzero address, does not wrap and, for a WoW64 client, lies below
// 4 GiB.
bool is_addressable(AddressRange range, bool is_wow64) noexcept;
bool is_addressable(const HostContext& context, AddressRange range) noexcept;

} // namespace directmetal::host
