#include "texture_descriptor.hpp"

#include "metal_options.hpp"

#include <array>

namespace directmetal::backend::resources {
namespace {

// Indexed by TextureType.
constexpr std::array<MTLTextureType, 9> texture_types{
    MTLTextureType1D,      MTLTextureType1DArray,       MTLTextureType2D,
    MTLTextureType2DArray, MTLTextureType2DMultisample, MTLTextureType2DMultisampleArray,
    MTLTextureType3D,      MTLTextureTypeCube,          MTLTextureTypeCubeArray,
};

} // namespace

// PixelFormat and TextureUsage values equal their Metal counterparts (pixel_format_check.mm).
MTLTextureDescriptor* texture_descriptor(const TextureDesc& desc) {
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor new];
    descriptor.textureType = texture_types[static_cast<std::size_t>(desc.type)];
    descriptor.pixelFormat = static_cast<MTLPixelFormat>(desc.format);
    descriptor.width = desc.extent.width;
    descriptor.height = desc.extent.height;
    descriptor.depth = desc.extent.depth;
    descriptor.mipmapLevelCount = desc.mip_levels;
    descriptor.arrayLength = desc.array_length;
    descriptor.sampleCount = desc.sample_count;
    descriptor.usage = static_cast<MTLTextureUsage>(desc.usage);
    descriptor.storageMode = metal_storage_mode(desc.storage);
    return descriptor;
}

} // namespace directmetal::backend::resources
