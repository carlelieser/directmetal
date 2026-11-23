#include "recorder.hpp"

#include "resource_access.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <vector>

// Clears are render passes whose attachments load with a clear value and store, one pass per
// mip, slice and, for 3D textures, depth plane.
namespace directmetal::backend::commands {
namespace {

struct ClearTarget {
    id<MTLTexture> texture;
    NSUInteger mip;
    NSUInteger slice;
    NSUInteger plane;
};

std::vector<ClearTarget> clear_targets(id<MTLTexture> texture, SubresourceRange range) {
    const bool is_volume = texture.textureType == MTLTextureType3D;
    std::vector<ClearTarget> targets;
    for (std::uint32_t mip = range.first_mip; mip < range.first_mip + range.mip_count; ++mip) {
        const NSUInteger planes = is_volume ? std::max<NSUInteger>(texture.depth >> mip, 1) : 1;
        const std::uint32_t end_slice = range.first_slice + range.slice_count;
        for (std::uint32_t slice = range.first_slice; slice < end_slice; ++slice) {
            for (NSUInteger plane = 0; plane < planes; ++plane) {
                targets.push_back({texture, mip, slice, plane});
            }
        }
    }
    return targets;
}

void attach(MTLRenderPassAttachmentDescriptor* attachment, const ClearTarget& target) {
    attachment.texture = target.texture;
    attachment.level = target.mip;
    attachment.slice = target.slice;
    attachment.depthPlane = target.plane;
    attachment.loadAction = MTLLoadActionClear;
    attachment.storeAction = MTLStoreActionStore;
}

double float_channel(std::uint32_t bits) {
    return std::bit_cast<float>(bits);
}

double uint_channel(std::uint32_t bits) {
    return bits;
}

double sint_channel(std::uint32_t bits) {
    return std::bit_cast<std::int32_t>(bits);
}

// Indexed by ClearKind.
constexpr std::array<double (*)(std::uint32_t), 3> channel_readers{
    float_channel,
    uint_channel,
    sint_channel,
};

MTLClearColor metal_clear_color(const ClearColor& color) {
    const auto read = channel_readers[static_cast<std::size_t>(color.kind)];
    return MTLClearColorMake(read(color.bits[0]), read(color.bits[1]), read(color.bits[2]),
                             read(color.bits[3]));
}

bool has_aspect(ClearAspect aspects, ClearAspect aspect) {
    return (static_cast<std::uint8_t>(aspects) & static_cast<std::uint8_t>(aspect)) != 0;
}

MTLRenderPassDescriptor* depth_stencil_pass(const ClearTarget& target,
                                            const DepthStencilClear& clear) {
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    if (has_aspect(clear.aspects, ClearAspect::depth)) {
        attach(pass.depthAttachment, target);
        pass.depthAttachment.clearDepth = clear.depth;
    }
    if (has_aspect(clear.aspects, ClearAspect::stencil)) {
        attach(pass.stencilAttachment, target);
        pass.stencilAttachment.clearStencil = clear.stencil;
    }
    return pass;
}

} // namespace

void MetalRecorder::clear_texture(Texture& texture, SubresourceRange range, ClearColor color) {
    @autoreleasepool {
        const MTLClearColor clear_color = metal_clear_color(color);
        for (const ClearTarget& target : clear_targets(resources::metal_texture(texture), range)) {
            MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
            attach(pass.colorAttachments[0], target);
            pass.colorAttachments[0].clearColor = clear_color;
            run_pass(pass);
        }
    }
}

void MetalRecorder::clear_depth_stencil(Texture& texture, SubresourceRange range,
                                        const DepthStencilClear& clear) {
    @autoreleasepool {
        for (const ClearTarget& target : clear_targets(resources::metal_texture(texture), range)) {
            run_pass(depth_stencil_pass(target, clear));
        }
    }
}

} // namespace directmetal::backend::commands
