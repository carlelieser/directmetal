#pragma once

#ifndef __OBJC__
#error "texture_descriptor.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include <directmetal/backend/types.hpp>

namespace directmetal::backend::resources {

MTLTextureDescriptor* texture_descriptor(const TextureDesc& desc);

} // namespace directmetal::backend::resources
