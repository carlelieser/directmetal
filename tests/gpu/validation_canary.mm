// Metal validation canary: proves that API validation is active in assert mode by encoding a blit
// copy larger than its source buffer. The debug layer rejects the copy on the CPU while it is
// encoded and aborts the process, so the tests built from this file are registered should_fail.
// The command buffer is never committed, so nothing reaches the GPU when validation is off; the
// canary then returns normally and its test fails.

#import <Metal/Metal.h>

#include <cstdio>

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

int main() {
    // 77 makes Meson report a skip rather than a failure where no GPU is available.
    constexpr int exit_skip = 77;
    return encode_oversized_copy() ? 0 : exit_skip;
}
