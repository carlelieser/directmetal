#include "resources.hpp"

#include "resource_access.hpp"

#include <utility>

namespace directmetal::backend::resources {
namespace {

std::byte* contents_of(id<MTLBuffer> buffer) {
    const bool is_cpu_visible = buffer.storageMode == MTLStorageModeShared;
    return is_cpu_visible ? static_cast<std::byte*>(buffer.contents) : nullptr;
}

} // namespace

NSString* label_string(std::string_view label) {
    return [[NSString alloc] initWithBytes:label.data()
                                    length:label.size()
                                  encoding:NSUTF8StringEncoding];
}

MetalBuffer::MetalBuffer(std::shared_ptr<Arena> arena, PlacedResource placed)
    : arena_(std::move(arena)), placed_(std::move(placed)),
      buffer_(static_cast<id<MTLBuffer>>(placed_.resource)), cpu_address_(contents_of(buffer_)) {}

MetalBuffer::MetalBuffer(id<MTLBuffer> imported)
    : placed_{}, buffer_(imported), cpu_address_(contents_of(imported)) {}

MetalBuffer::~MetalBuffer() {
    buffer_ = nil;
    if (arena_) {
        arena_->release(std::move(placed_));
    }
}

void MetalBuffer::set_label(std::string_view label) {
    buffer_.label = label_string(label);
}

MetalTexture::MetalTexture(std::shared_ptr<Arena> arena, PlacedResource placed,
                           const TextureDesc& desc)
    : arena_(std::move(arena)), placed_(std::move(placed)),
      texture_(static_cast<id<MTLTexture>>(placed_.resource)), desc_(desc) {}

MetalTexture::~MetalTexture() {
    texture_ = nil;
    arena_->release(std::move(placed_));
}

void MetalTexture::set_label(std::string_view label) {
    texture_.label = label_string(label);
}

id<MTLBuffer> metal_buffer(Buffer& buffer) noexcept {
    return static_cast<MetalBuffer&>(buffer).metal();
}

id<MTLTexture> metal_texture(Texture& texture) noexcept {
    return static_cast<MetalTexture&>(texture).metal();
}

} // namespace directmetal::backend::resources
