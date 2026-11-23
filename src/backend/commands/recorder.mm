#include "recorder.hpp"

#include "resource_access.hpp"

#include <utility>

namespace directmetal::backend::commands {
namespace {

using resources::metal_buffer;
using resources::metal_texture;

MTLOrigin metal_origin(Origin3D origin) {
    return MTLOriginMake(origin.x, origin.y, origin.z);
}

MTLSize metal_size(Extent3D extent) {
    return MTLSizeMake(extent.width, extent.height, extent.depth);
}

// Buffer copies of a combined depth-stencil format move its depth aspect; the interface names no
// aspect, and Metal rejects such copies without one.
MTLBlitOption blit_option(id<MTLTexture> texture) {
    const MTLPixelFormat format = texture.pixelFormat;
    const bool is_combined = format == MTLPixelFormatDepth32Float_Stencil8 ||
                             format == MTLPixelFormatDepth24Unorm_Stencil8;
    return is_combined ? MTLBlitOptionDepthFromDepthStencil : MTLBlitOptionNone;
}

} // namespace

MetalRecorder::MetalRecorder(id<MTLCommandQueue> queue) : queue_(queue) {}

MetalRecorder::~MetalRecorder() {
    @autoreleasepool {
        end_encoding();
    }
}

id<MTLCommandBuffer> MetalRecorder::command_buffer() {
    if (commands_ == nil) {
        commands_ = [queue_ commandBuffer];
    }
    return commands_;
}

id<MTLBlitCommandEncoder> MetalRecorder::blit_encoder() {
    if (blit_ == nil) {
        blit_ = [command_buffer() blitCommandEncoder];
    }
    return blit_;
}

void MetalRecorder::end_encoding() {
    if (blit_ != nil) {
        [blit_ endEncoding];
        blit_ = nil;
    }
}

void MetalRecorder::run_pass(MTLRenderPassDescriptor* pass) {
    end_encoding();
    id<MTLRenderCommandEncoder> encoder =
        [command_buffer() renderCommandEncoderWithDescriptor:pass];
    [encoder endEncoding];
}

void MetalRecorder::copy_buffer(const BufferCopy& copy) {
    @autoreleasepool {
        [blit_encoder() copyFromBuffer:metal_buffer(copy.source)
                          sourceOffset:copy.source_offset
                              toBuffer:metal_buffer(copy.destination)
                     destinationOffset:copy.destination_offset
                                  size:copy.size];
    }
}

void MetalRecorder::copy_buffer_to_texture(const BufferTextureCopy& copy) {
    @autoreleasepool {
        id<MTLTexture> texture = metal_texture(copy.texture);
        [blit_encoder() copyFromBuffer:metal_buffer(copy.buffer)
                          sourceOffset:copy.offset
                     sourceBytesPerRow:copy.row_pitch
                   sourceBytesPerImage:copy.slice_pitch
                            sourceSize:metal_size(copy.extent)
                             toTexture:texture
                      destinationSlice:copy.slice
                      destinationLevel:copy.mip
                     destinationOrigin:metal_origin(copy.origin)
                               options:blit_option(texture)];
    }
}

void MetalRecorder::copy_texture_to_buffer(const BufferTextureCopy& copy) {
    @autoreleasepool {
        id<MTLTexture> texture = metal_texture(copy.texture);
        [blit_encoder() copyFromTexture:texture
                            sourceSlice:copy.slice
                            sourceLevel:copy.mip
                           sourceOrigin:metal_origin(copy.origin)
                             sourceSize:metal_size(copy.extent)
                               toBuffer:metal_buffer(copy.buffer)
                      destinationOffset:copy.offset
                 destinationBytesPerRow:copy.row_pitch
               destinationBytesPerImage:copy.slice_pitch
                                options:blit_option(texture)];
    }
}

void MetalRecorder::copy_texture(const TextureCopy& copy) {
    @autoreleasepool {
        [blit_encoder() copyFromTexture:metal_texture(copy.source)
                            sourceSlice:copy.source_slice
                            sourceLevel:copy.source_mip
                           sourceOrigin:metal_origin(copy.source_origin)
                             sourceSize:metal_size(copy.extent)
                              toTexture:metal_texture(copy.destination)
                       destinationSlice:copy.destination_slice
                       destinationLevel:copy.destination_mip
                      destinationOrigin:metal_origin(copy.destination_origin)];
    }
}

void MetalRecorder::signal(Timeline& timeline, std::uint64_t value) {
    @autoreleasepool {
        end_encoding();
        const std::shared_ptr<TimelineState>& state = timeline_state(timeline);
        [command_buffer() encodeSignalEvent:state->event() value:value];
        signaled_.push_back(state);
    }
}

RecordedWork MetalRecorder::finish() {
    end_encoding();
    RecordedWork work{command_buffer(), std::move(signaled_)};
    commands_ = nil;
    signaled_.clear();
    return work;
}

} // namespace directmetal::backend::commands
