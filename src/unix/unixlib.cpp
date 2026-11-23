#include <directmetal/host/dispatch.hpp>
#include <directmetal/unixlib.hpp>

#include <cstdint>
#include <iterator>

namespace directmetal {

template <std::uint32_t Code, host::CallerAbi Abi> bridge::Status entry(void* params) noexcept {
    return host::dispatch(Code, params, Abi);
}

template <std::uint32_t Code>
inline constexpr UnixCallEntry native = entry<Code, host::CallerAbi::native>;
template <std::uint32_t Code>
inline constexpr UnixCallEntry wow64 = entry<Code, host::CallerAbi::wow64>;

} // namespace directmetal

// Argument layouts are identical for 32-bit and 64-bit callers; the WoW64 table only tells the host
// that addresses must lie below 4 GiB. Wine
// indexes them by bridge::FunctionCode; append an entry whenever a code is added.
extern "C" {
const directmetal::UnixCallEntry __wine_unix_call_funcs[] = {
    directmetal::native<0>,  directmetal::native<1>,  directmetal::native<2>,
    directmetal::native<3>,  directmetal::native<4>,  directmetal::native<5>,
    directmetal::native<6>,  directmetal::native<7>,  directmetal::native<8>,
    directmetal::native<9>,  directmetal::native<10>, directmetal::native<11>,
    directmetal::native<12>, directmetal::native<13>, directmetal::native<14>,
    directmetal::native<15>,
};
const directmetal::UnixCallEntry __wine_unix_call_wow64_funcs[] = {
    directmetal::wow64<0>,  directmetal::wow64<1>,  directmetal::wow64<2>,  directmetal::wow64<3>,
    directmetal::wow64<4>,  directmetal::wow64<5>,  directmetal::wow64<6>,  directmetal::wow64<7>,
    directmetal::wow64<8>,  directmetal::wow64<9>,  directmetal::wow64<10>, directmetal::wow64<11>,
    directmetal::wow64<12>, directmetal::wow64<13>, directmetal::wow64<14>, directmetal::wow64<15>,
};
}

static_assert(std::size(__wine_unix_call_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
static_assert(std::size(__wine_unix_call_wow64_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
