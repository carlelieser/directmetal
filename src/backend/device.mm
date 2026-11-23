#include "adapters.hpp"
#include "arena.hpp"
#include "commands/factory.hpp"
#include "metal_options.hpp"
#include "resource_access.hpp"
#include "resources.hpp"
#include "texture_descriptor.hpp"

#include <cassert>
#include <memory>

namespace directmetal::backend {
namespace {

using resources::Arena;
using resources::MemoryRequirement;
using resources::PlacedResource;

template <typename T> Result<std::unique_ptr<T>> fail(Status status) {
    return Result<std::unique_ptr<T>>::failure(status);
}

class MetalDevice final : public Device {
public:
    MetalDevice(id<MTLDevice> device, const Capabilities& capabilities,
                const DeviceRequest& request)
        : arena_(std::make_shared<Arena>(device, request)), capabilities_(capabilities) {}

    const Capabilities& capabilities() const noexcept override { return capabilities_; }
    Result<std::unique_ptr<Buffer>> create_buffer(const BufferDesc& desc) override;
    Result<std::unique_ptr<Buffer>> import_memory(std::span<std::byte> memory) override;
    Result<std::unique_ptr<Texture>> create_texture(const TextureDesc& desc) override;

    Result<std::unique_ptr<Timeline>> create_timeline(std::uint64_t initial_value) override {
        return commands::create_timeline(*this, initial_value);
    }

    Result<std::unique_ptr<Queue>> create_queue() override { return commands::create_queue(*this); }
    MemoryStats memory_stats() const override { return arena_->stats(); }
    Arena& arena() noexcept { return *arena_; }

private:
    std::shared_ptr<Arena> arena_;
    Capabilities capabilities_;
};

Result<std::unique_ptr<Buffer>> MetalDevice::create_buffer(const BufferDesc& desc) {
    @autoreleasepool {
        assert(desc.size > 0);
        if (desc.storage == StorageMode::memoryless) {
            return fail<Buffer>(Status::unsupported);
        }
        id<MTLDevice> device = arena_->device();
        const MTLResourceOptions options = resources::metal_resource_options(desc.storage);
        const NSUInteger length = desc.size;
        const MTLSizeAndAlign need = [device heapBufferSizeAndAlignWithLength:length
                                                                      options:options];
        auto placed =
            arena_->place({need.size, need.align, desc.storage},
                          ^id<MTLResource>(id<MTLHeap> heap, NSUInteger offset) {
                            if (heap == nil) {
                                return [device newBufferWithLength:length options:options];
                            }
                            return [heap newBufferWithLength:length options:options offset:offset];
                          });
        if (!placed.has_value()) {
            return fail<Buffer>(placed.status());
        }
        return Result<std::unique_ptr<Buffer>>::success(
            std::make_unique<resources::MetalBuffer>(arena_, std::move(placed).value()));
    }
}

Result<std::unique_ptr<Buffer>> MetalDevice::import_memory(std::span<std::byte> memory) {
    @autoreleasepool {
        const auto address = reinterpret_cast<std::uintptr_t>(memory.data());
        assert(address % import_alignment == 0);
        assert(!memory.empty() && memory.size() % import_alignment == 0);
        id<MTLBuffer> buffer =
            [arena_->device() newBufferWithBytesNoCopy:memory.data()
                                                length:memory.size()
                                               options:MTLResourceStorageModeShared
                                           deallocator:nil];
        if (buffer == nil) {
            return fail<Buffer>(Status::out_of_memory);
        }
        return Result<std::unique_ptr<Buffer>>::success(
            std::make_unique<resources::MetalBuffer>(buffer));
    }
}

// Memoryless textures have no backing memory, so they need no size query and are never placed.
Result<std::unique_ptr<Texture>> MetalDevice::create_texture(const TextureDesc& desc) {
    @autoreleasepool {
        id<MTLDevice> device = arena_->device();
        MTLTextureDescriptor* descriptor = resources::texture_descriptor(desc);
        MemoryRequirement requirement{0, 1, desc.storage};
        if (desc.storage != StorageMode::memoryless) {
            const MTLSizeAndAlign need = [device heapTextureSizeAndAlignWithDescriptor:descriptor];
            requirement = {need.size, need.align, desc.storage};
        }
        auto placed =
            arena_->place(requirement, ^id<MTLResource>(id<MTLHeap> heap, NSUInteger offset) {
              if (heap == nil) {
                  return [device newTextureWithDescriptor:descriptor];
              }
              return [heap newTextureWithDescriptor:descriptor offset:offset];
            });
        if (!placed.has_value()) {
            return fail<Texture>(placed.status());
        }
        return Result<std::unique_ptr<Texture>>::success(
            std::make_unique<resources::MetalTexture>(arena_, std::move(placed).value(), desc));
    }
}

} // namespace

// Creates the Metal device on demand; never called while directmetal.so loads.
Result<std::unique_ptr<Device>> create_device(const DeviceRequest& request) {
    assert(request.heap_block_size > 0);
    @autoreleasepool {
        id<MTLDevice> device = adapters::select(request.registry_id);
        if (device == nil) {
            return fail<Device>(Status::unsupported);
        }
        const auto capabilities = adapters::describe(device);
        if (!capabilities) {
            return fail<Device>(Status::unsupported);
        }
        return Result<std::unique_ptr<Device>>::success(
            std::make_unique<MetalDevice>(device, *capabilities, request));
    }
}

} // namespace directmetal::backend

namespace directmetal::backend::resources {

id<MTLDevice> metal_device(Device& device) noexcept {
    return static_cast<MetalDevice&>(device).arena().device();
}

void track_queue(Device& device, Queue& queue) {
    static_cast<MetalDevice&>(device).arena().track(queue);
}

void untrack_queue(Device& device, Queue& queue) noexcept {
    static_cast<MetalDevice&>(device).arena().untrack(queue);
}

} // namespace directmetal::backend::resources
