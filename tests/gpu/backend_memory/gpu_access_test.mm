#include "device_fixture.hpp"
#include "resource_access.hpp"

#include <directmetal/backend/types.hpp>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <numeric>
#include <vector>

#include <sys/mman.h>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

struct AlignedFree {
    void operator()(std::byte* memory) const noexcept { std::free(memory); }
};

using CallerMemory = std::unique_ptr<std::byte[], AlignedFree>;

CallerMemory allocate_caller_memory(std::size_t size) {
    return CallerMemory(static_cast<std::byte*>(std::aligned_alloc(import_alignment, size)));
}

// Runs blits on a queue of the backend's device; the command lane's queues replace this later.
void run_blits(Device& device, void (^encode)(id<MTLBlitCommandEncoder>)) {
    id<MTLCommandQueue> queue = [resources::metal_device(device) newCommandQueue];
    id<MTLCommandBuffer> commands = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    encode(blit);
    [blit endEncoding];
    [commands commit];
    [commands waitUntilCompleted];
    ASSERT_EQ(commands.status, MTLCommandBufferStatusCompleted);
}

void fill_pattern(std::byte* bytes, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        bytes[index] = static_cast<std::byte>((index * 7 + 3) & 0xff);
    }
}

TEST(ImportMemory, WrapsCallerMemoryWithoutCopying) {
    const auto device = make_device();
    constexpr std::size_t size = 4 * import_alignment;
    const CallerMemory memory = allocate_caller_memory(size);
    auto imported = device->import_memory({memory.get(), size});
    ASSERT_TRUE(imported.has_value());
    Buffer& buffer = *imported.value();
    EXPECT_EQ(buffer.size(), size);
    EXPECT_EQ(buffer.cpu_address(), memory.get());
    EXPECT_EQ(resources::metal_buffer(buffer).contents, memory.get());
    EXPECT_EQ(resources::metal_buffer(buffer).heap, nil);
    EXPECT_EQ(device->memory_stats().used_bytes, 0u);
}

TEST(ImportMemory, GpuReadsAndWritesCallerMemory) {
    const auto device = make_device();
    constexpr std::size_t size = import_alignment;
    const CallerMemory source_memory = allocate_caller_memory(size);
    const CallerMemory destination_memory = allocate_caller_memory(size);
    fill_pattern(source_memory.get(), size);
    std::memset(destination_memory.get(), 0, size);
    auto source = device->import_memory({source_memory.get(), size});
    auto destination = device->import_memory({destination_memory.get(), size});
    const auto staging = make_buffer(*device, size, StorageMode::gpu_private);
    id<MTLBuffer> from = resources::metal_buffer(*source.value());
    id<MTLBuffer> to = resources::metal_buffer(*destination.value());
    id<MTLBuffer> middle = resources::metal_buffer(*staging);
    run_blits(*device, ^(id<MTLBlitCommandEncoder> blit) {
      [blit copyFromBuffer:from sourceOffset:0 toBuffer:middle destinationOffset:0 size:size];
      [blit copyFromBuffer:middle sourceOffset:0 toBuffer:to destinationOffset:0 size:size];
    });
    EXPECT_EQ(std::memcmp(source_memory.get(), destination_memory.get(), size), 0);
}

// Maps an inaccessible region aligned to import_alignment. Pages are 4 KiB under Rosetta, so
// mmap alone may return a misaligned address: map one extra alignment and unmap the excess.
std::byte* map_inaccessible(std::size_t size) {
    const std::size_t mapped = size + import_alignment;
    void* pages = mmap(nullptr, mapped, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (pages == MAP_FAILED) {
        return nullptr;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(pages);
    const std::uintptr_t aligned = (start + import_alignment - 1) & ~(import_alignment - 1);
    const std::size_t head = aligned - start;
    if (head > 0) {
        munmap(pages, head);
    }
    const std::size_t tail = mapped - head - size;
    if (tail > 0) {
        munmap(reinterpret_cast<void*>(aligned + size), tail);
    }
    return reinterpret_cast<std::byte*>(aligned);
}

TEST(ImportMemory, RejectsInaccessibleMemory) {
    constexpr std::size_t size = import_alignment;
    std::byte* const pages = map_inaccessible(size);
    ASSERT_NE(pages, nullptr);
    const auto device = make_device();
    const auto imported = device->import_memory({pages, size});
    ASSERT_FALSE(imported.has_value());
    EXPECT_EQ(imported.status(), Status::invalid_argument);
    munmap(pages, size);
}

TEST(HeapResources, RoundTripThroughPrivateTexture) {
    const auto device = make_device();
    constexpr std::uint32_t extent = 16;
    constexpr std::size_t row_pitch = extent * 4;
    constexpr std::size_t size = row_pitch * extent;
    const auto upload = make_buffer(*device, size);
    const auto readback = make_buffer(*device, size);
    fill_pattern(upload->cpu_address(), size);
    std::memset(readback->cpu_address(), 0, size);
    const auto texture = make_texture(
        *device, texture_2d(PixelFormat::rgba8_uint, extent, StorageMode::gpu_private));
    id<MTLBuffer> from = resources::metal_buffer(*upload);
    id<MTLBuffer> to = resources::metal_buffer(*readback);
    id<MTLTexture> image = resources::metal_texture(*texture);
    run_blits(*device, ^(id<MTLBlitCommandEncoder> blit) {
      const MTLSize region = MTLSizeMake(extent, extent, 1);
      [blit copyFromBuffer:from
                 sourceOffset:0
            sourceBytesPerRow:row_pitch
          sourceBytesPerImage:size
                   sourceSize:region
                    toTexture:image
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
      [blit copyFromTexture:image
                       sourceSlice:0
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:region
                          toBuffer:to
                 destinationOffset:0
            destinationBytesPerRow:row_pitch
          destinationBytesPerImage:size];
    });
    EXPECT_EQ(std::memcmp(upload->cpu_address(), readback->cpu_address(), size), 0);
}

} // namespace
