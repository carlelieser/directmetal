#include "commands_fixture.hpp"

#include <bit>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

ClearColor uint_color(std::uint32_t value) {
    return {{value, value + 1, value + 2, value + 3}, ClearKind::uint_value, {}};
}

std::vector<std::byte> uint_pixels(std::uint32_t value, std::size_t pixel_count) {
    std::vector<std::byte> bytes;
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            bytes.push_back(static_cast<std::byte>(value + channel));
        }
    }
    return bytes;
}

constexpr std::uint32_t background = 10;
constexpr std::uint32_t foreground = 200;

class SubresourceClear : public CommandsTest {
protected:
    // Reads every pixel of one subresource of an rgba8_uint texture.
    std::vector<std::byte> read(Texture& texture, std::uint32_t mip, std::uint32_t slice) {
        const TextureDesc& desc = texture.desc();
        const bool is_volume = desc.type == TextureType::tex3d;
        const Extent3D extent{std::max(desc.extent.width >> mip, 1u),
                              std::max(desc.extent.height >> mip, 1u),
                              is_volume ? std::max(desc.extent.depth >> mip, 1u) : 1u};
        return read_texture(texture, {mip, slice, extent, 4});
    }

    static std::size_t pixel_count(const std::vector<std::byte>& bytes) { return bytes.size() / 4; }
};

TEST_F(SubresourceClear, ClearsOnlyTheNamedMipsAndSlices) {
    TextureDesc desc = texture_desc(TextureType::tex2d_array, PixelFormat::rgba8_uint, {8, 8, 1});
    desc.mip_levels = 3;
    desc.array_length = 3;
    const auto texture = make_texture(desc);
    recorder().clear_texture(*texture, {0, 3, 0, 3}, uint_color(background));
    recorder().clear_texture(*texture, {1, 1, 1, 2}, uint_color(foreground));
    for (std::uint32_t mip = 0; mip < 3; ++mip) {
        for (std::uint32_t slice = 0; slice < 3; ++slice) {
            const auto bytes = read(*texture, mip, slice);
            const bool is_cleared = mip == 1 && slice >= 1;
            const std::uint32_t value = is_cleared ? foreground : background;
            EXPECT_EQ(bytes, uint_pixels(value, pixel_count(bytes))) << mip << "/" << slice;
        }
    }
}

TEST_F(SubresourceClear, ClearsEveryDepthPlaneOfAVolumeMip) {
    TextureDesc desc = texture_desc(TextureType::tex3d, PixelFormat::rgba8_uint, {4, 4, 4});
    desc.mip_levels = 2;
    const auto texture = make_texture(desc);
    recorder().clear_texture(*texture, {0, 1, 0, 1}, uint_color(background));
    recorder().clear_texture(*texture, {1, 1, 0, 1}, uint_color(foreground));
    const auto base = read(*texture, 0, 0);
    EXPECT_EQ(pixel_count(base), 64u);
    EXPECT_EQ(base, uint_pixels(background, 64));
    const auto mip = read(*texture, 1, 0);
    EXPECT_EQ(pixel_count(mip), 8u);
    EXPECT_EQ(mip, uint_pixels(foreground, 8));
}

TEST_F(SubresourceClear, ClearsOneCubeFace) {
    const auto texture =
        make_texture(texture_desc(TextureType::cube, PixelFormat::rgba8_uint, {4, 4, 1}));
    recorder().clear_texture(*texture, {0, 1, 0, 6}, uint_color(background));
    recorder().clear_texture(*texture, {0, 1, 3, 1}, uint_color(foreground));
    for (std::uint32_t face = 0; face < 6; ++face) {
        const std::uint32_t value = face == 3 ? foreground : background;
        EXPECT_EQ(read(*texture, 0, face), uint_pixels(value, 16)) << face;
    }
}

} // namespace
