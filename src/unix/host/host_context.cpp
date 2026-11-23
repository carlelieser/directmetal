#include "host_context.hpp"

#include <limits>

namespace directmetal::host {

bool is_addressable(AddressRange range, bool is_wow64) noexcept {
    const std::uint64_t limit =
        is_wow64 ? std::uint64_t{1} << 32 : std::numeric_limits<std::uintptr_t>::max();
    const bool is_start_valid = range.address != 0 && range.address < limit;
    return is_start_valid && range.size <= limit - range.address;
}

bool is_addressable(const HostContext& context, AddressRange range) noexcept {
    return is_addressable(range, context.is_wow64.load());
}

} // namespace directmetal::host
