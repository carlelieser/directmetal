// Metal validation canary: proves that API validation is active in assert mode by encoding a blit
// copy larger than its source buffer. The debug layer rejects the copy on the CPU while it is
// encoded and aborts the process; the tests require that abort and its message. The command
// buffer is never committed, so nothing reaches the GPU when validation is off; the canary then
// returns normally and its test fails.
//
// Built as a native executable, and as a stand-in for directmetal.so that encodes the copy on its
// first unix call from a Wine process and otherwise forwards to host::dispatch. Wine turns the
// abort into a failed unix call (status 0xc0000005), so the PE program fails instead.

#import <Metal/Metal.h>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr NSUInteger source_length = 256;
constexpr NSUInteger copy_size = source_length * 2;

// Returns false when the process has no Metal device.
bool encode_oversized_copy() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            std::fprintf(stderr, "validation_canary: no Metal device\n");
            return false;
        }
        id<MTLBuffer> source = [device newBufferWithLength:source_length
                                                   options:MTLResourceStorageModeShared];
        id<MTLBuffer> destination = [device newBufferWithLength:copy_size
                                                        options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> commands = [[device newCommandQueue] commandBuffer];
        id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
        [blit copyFromBuffer:source
                 sourceOffset:0
                     toBuffer:destination
            destinationOffset:0
                         size:copy_size];
        [blit endEncoding];
    }
    std::fprintf(stderr, "validation_canary: Metal validation accepted an oversized copy\n");
    return true;
}

} // namespace

#ifdef DIRECTMETAL_VALIDATION_CANARY_UNIXLIB

#include <directmetal/host/dispatch.hpp>
#include <directmetal/unixlib.hpp>

#include <cstdint>
#include <iterator>
#include <mutex>

namespace {

std::once_flag canary_once;

template <std::uint32_t Code> directmetal::bridge::Status entry(void* params) noexcept {
    std::call_once(canary_once, encode_oversized_copy);
    return directmetal::host::dispatch(Code, params);
}

} // namespace

extern "C" {
const directmetal::UnixCallEntry __wine_unix_call_funcs[] = {
    entry<0>, entry<1>, entry<2>,  entry<3>,  entry<4>,  entry<5>,  entry<6>,  entry<7>,
    entry<8>, entry<9>, entry<10>, entry<11>, entry<12>, entry<13>, entry<14>, entry<15>,
};
const directmetal::UnixCallEntry __wine_unix_call_wow64_funcs[] = {
    entry<0>, entry<1>, entry<2>,  entry<3>,  entry<4>,  entry<5>,  entry<6>,  entry<7>,
    entry<8>, entry<9>, entry<10>, entry<11>, entry<12>, entry<13>, entry<14>, entry<15>,
};
}

static_assert(std::size(__wine_unix_call_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));
static_assert(std::size(__wine_unix_call_wow64_funcs) ==
              directmetal::bridge::to_index(directmetal::bridge::FunctionCode::count));

#else

int main() {
    return encode_oversized_copy() ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
