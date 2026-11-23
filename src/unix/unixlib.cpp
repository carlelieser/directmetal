#include <directmetal/host/dispatch.hpp>
#include <directmetal/unixlib.hpp>

#include <cstdint>
#include <iterator>

namespace directmetal {

template <std::uint32_t Code> bridge::Status entry(void* params) noexcept {
    return host::dispatch(Code, params);
}

} // namespace directmetal

// Argument layouts are identical for 32-bit and 64-bit callers, so both tables share entries. Wine
// indexes them by bridge::FunctionCode; append an entry whenever a code is added.
extern "C" {
const directmetal::UnixCallEntry __wine_unix_call_funcs[] = {
    directmetal::entry<0>,  directmetal::entry<1>,  directmetal::entry<2>,  directmetal::entry<3>,
    directmetal::entry<4>,  directmetal::entry<5>,  directmetal::entry<6>,  directmetal::entry<7>,
    directmetal::entry<8>,  directmetal::entry<9>,  directmetal::entry<10>, directmetal::entry<11>,
    directmetal::entry<12>, directmetal::entry<13>, directmetal::entry<14>, directmetal::entry<15>,
};
const directmetal::UnixCallEntry __wine_unix_call_wow64_funcs[] = {
    directmetal::entry<0>,  directmetal::entry<1>,  directmetal::entry<2>,  directmetal::entry<3>,
    directmetal::entry<4>,  directmetal::entry<5>,  directmetal::entry<6>,  directmetal::entry<7>,
    directmetal::entry<8>,  directmetal::entry<9>,  directmetal::entry<10>, directmetal::entry<11>,
    directmetal::entry<12>, directmetal::entry<13>, directmetal::entry<14>, directmetal::entry<15>,
};
}

static_assert(std::size(__wine_unix_call_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
static_assert(std::size(__wine_unix_call_wow64_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
