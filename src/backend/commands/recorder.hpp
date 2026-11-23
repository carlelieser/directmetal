#pragma once

#ifndef __OBJC__
#error "recorder.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include "timeline.hpp"

#include <directmetal/backend/commands.hpp>

#include <memory>
#include <vector>

namespace directmetal::backend::commands {

// A command buffer ready to commit and the timelines it signals.
struct RecordedWork {
    id<MTLCommandBuffer> commands;
    std::vector<std::shared_ptr<TimelineState>> signaled;
};

// Records into one MTLCommandBuffer at a time, created on first use. Consecutive copies share a
// blit encoder; each cleared subresource gets its own render pass.
class MetalRecorder final : public CommandRecorder {
public:
    explicit MetalRecorder(id<MTLCommandQueue> queue);
    MetalRecorder(const MetalRecorder&) = delete;
    MetalRecorder& operator=(const MetalRecorder&) = delete;
    ~MetalRecorder() override;

    void clear_texture(Texture& texture, SubresourceRange range, ClearColor color) override;
    void clear_depth_stencil(Texture& texture, SubresourceRange range,
                             const DepthStencilClear& clear) override;
    void copy_buffer(const BufferCopy& copy) override;
    void copy_buffer_to_texture(const BufferTextureCopy& copy) override;
    void copy_texture_to_buffer(const BufferTextureCopy& copy) override;
    void copy_texture(const TextureCopy& copy) override;
    void signal(Timeline& timeline, std::uint64_t value) override;

    // Makes later commands wait on the GPU until timeline reaches value. Not part of
    // CommandRecorder yet; test_seams.mm uses it.
    void wait(Timeline& timeline, std::uint64_t value);

    // Ends recording; the next command starts a new command buffer.
    RecordedWork finish();

private:
    id<MTLCommandBuffer> command_buffer();
    id<MTLBlitCommandEncoder> blit_encoder();
    void end_encoding();
    void run_pass(MTLRenderPassDescriptor* pass);

    id<MTLCommandQueue> queue_;
    id<MTLCommandBuffer> commands_ = nil;
    id<MTLBlitCommandEncoder> blit_ = nil;
    std::vector<std::shared_ptr<TimelineState>> signaled_;
};

} // namespace directmetal::backend::commands
