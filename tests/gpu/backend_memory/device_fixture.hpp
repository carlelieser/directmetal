#pragma once

#include <directmetal/backend/device.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

namespace directmetal::backend::testing {

inline constexpr std::uint64_t kib = 1024;
inline constexpr std::uint64_t mib = 1024 * kib;

inline std::unique_ptr<Device> make_device(const DeviceRequest& request = {}) {
    auto device = create_device(request);
    EXPECT_TRUE(device.has_value()) << static_cast<int>(device.status());
    return device.has_value() ? std::move(device).value() : nullptr;
}

inline std::unique_ptr<Buffer> make_buffer(Device& device, std::uint64_t size,
                                           StorageMode storage = StorageMode::shared) {
    auto buffer = device.create_buffer({size, storage, {}});
    EXPECT_TRUE(buffer.has_value()) << static_cast<int>(buffer.status());
    return buffer.has_value() ? std::move(buffer).value() : nullptr;
}

inline TextureDesc texture_2d(PixelFormat format, std::uint32_t size, StorageMode storage) {
    return {TextureType::tex2d,
            storage,
            0,
            format,
            {size, size, 1},
            1,
            1,
            1,
            TextureUsage::shader_read | TextureUsage::render_target};
}

inline std::unique_ptr<Texture> make_texture(Device& device, const TextureDesc& desc) {
    auto texture = device.create_texture(desc);
    EXPECT_TRUE(texture.has_value()) << static_cast<int>(texture.status());
    return texture.has_value() ? std::move(texture).value() : nullptr;
}

} // namespace directmetal::backend::testing
