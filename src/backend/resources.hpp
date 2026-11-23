#pragma once

#ifndef __OBJC__
#error "resources.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include "arena.hpp"

#include <directmetal/backend/commands.hpp>

#include <memory>

namespace directmetal::backend::resources {

class MetalBuffer final : public Buffer {
public:
    // A buffer whose memory the arena returns on destruction.
    MetalBuffer(std::shared_ptr<Arena> arena, PlacedResource placed);
    // A buffer over caller-owned memory, released immediately on destruction.
    explicit MetalBuffer(id<MTLBuffer> imported);
    ~MetalBuffer() override;

    std::uint64_t size() const noexcept override { return buffer_.length; }
    std::byte* cpu_address() noexcept override { return cpu_address_; }
    void set_label(std::string_view label) override;
    id<MTLBuffer> metal() const noexcept { return buffer_; }

private:
    std::shared_ptr<Arena> arena_;
    PlacedResource placed_;
    id<MTLBuffer> buffer_;
    std::byte* cpu_address_;
};

class MetalTexture final : public Texture {
public:
    MetalTexture(std::shared_ptr<Arena> arena, PlacedResource placed, const TextureDesc& desc);
    ~MetalTexture() override;

    const TextureDesc& desc() const noexcept override { return desc_; }
    void set_label(std::string_view label) override;
    id<MTLTexture> metal() const noexcept { return texture_; }

private:
    std::shared_ptr<Arena> arena_;
    PlacedResource placed_;
    id<MTLTexture> texture_;
    TextureDesc desc_;
};

NSString* label_string(std::string_view label);

} // namespace directmetal::backend::resources
