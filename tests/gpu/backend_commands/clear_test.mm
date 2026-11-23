#include "commands_fixture.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <string>

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

constexpr std::uint32_t extent = 8;
constexpr SubresourceRange whole{0, 1, 0, 1};

struct ClearCase {
    std::string name;
    PixelFormat format;
    ClearColor color;
    // One pixel as stored.
    std::vector<std::uint8_t> expected;
    int tolerance;
};

ClearColor float_color(std::array<float, 4> channels) {
    ClearColor color{{}, ClearKind::float_value, {}};
    for (std::size_t index = 0; index < channels.size(); ++index) {
        color.bits[index] = std::bit_cast<std::uint32_t>(channels[index]);
    }
    return color;
}

template <typename T> std::vector<std::uint8_t> bytes_of(std::array<T, 4> channels) {
    std::vector<std::uint8_t> bytes(sizeof(channels));
    std::memcpy(bytes.data(), channels.data(), sizeof(channels));
    return bytes;
}

std::vector<ClearCase> clear_cases() {
    const std::array<float, 4> wide{1.5f, -3.25f, 0.0f, 65536.0f};
    return {
        {"unorm",
         PixelFormat::rgba8_unorm,
         float_color({0.2f, 0.4f, 0.6f, 0.8f}),
         {51, 102, 153, 204},
         1},
        {"unorm_bgra",
         PixelFormat::bgra8_unorm,
         float_color({0.2f, 0.4f, 0.6f, 0.8f}),
         {153, 102, 51, 204},
         1},
        // Linear 0.25, 0.1 and 0.75 encode to sRGB 136.9, 89.1 and 224.6 of 255.
        {"srgb",
         PixelFormat::rgba8_unorm_srgb,
         float_color({0.25f, 0.1f, 0.75f, 0.6f}),
         {137, 89, 225, 153},
         1},
        {"half_float", PixelFormat::rgba16_float, float_color({0.5f, -2.0f, 1024.0f, 0.25f}),
         bytes_of<std::uint16_t>({0x3800, 0xc000, 0x6400, 0x3400}), 0},
        {"float", PixelFormat::rgba32_float, float_color(wide), bytes_of<float>(wide), 0},
        {"uint",
         PixelFormat::rgba32_uint,
         {{0xdeadbeef, 7, 0, 0xffffffff}, ClearKind::uint_value, {}},
         bytes_of<std::uint32_t>({0xdeadbeef, 7, 0, 0xffffffff}),
         0},
        {"sint",
         PixelFormat::rgba32_sint,
         {{std::bit_cast<std::uint32_t>(-5), 123456, std::bit_cast<std::uint32_t>(INT32_MIN), 1},
          ClearKind::sint_value,
          {}},
         bytes_of<std::int32_t>({-5, 123456, INT32_MIN, 1}),
         0},
    };
}

class ClearColorTest : public CommandsTest, public ::testing::WithParamInterface<ClearCase> {};

void expect_pixels(const std::vector<std::byte>& bytes, const std::vector<std::uint8_t>& pixel,
                   int tolerance) {
    ASSERT_EQ(bytes.size() % pixel.size(), 0u);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const int actual = std::to_integer<int>(bytes[index]);
        const int expected = pixel[index % pixel.size()];
        ASSERT_LE(std::abs(actual - expected), tolerance) << "byte " << index;
    }
}

TEST_P(ClearColorTest, ReadsBackClearedColor) {
    const ClearCase& clear = GetParam();
    const auto texture =
        make_texture(texture_desc(TextureType::tex2d, clear.format, {extent, extent, 1}));
    recorder().clear_texture(*texture, whole, clear.color);
    const auto bytes = read_texture(
        *texture, {0, 0, {extent, extent, 1}, static_cast<std::uint32_t>(clear.expected.size())});
    expect_pixels(bytes, clear.expected, clear.tolerance);
}

INSTANTIATE_TEST_SUITE_P(FormatKinds, ClearColorTest, ::testing::ValuesIn(clear_cases()),
                         [](const auto& info) { return info.param.name; });

TEST_F(CommandsTest, ClearsDepth) {
    const auto texture = make_texture(
        texture_desc(TextureType::tex2d, PixelFormat::depth32_float, {extent, extent, 1}));
    recorder().clear_depth_stencil(*texture, whole, {0.375f, 0, ClearAspect::depth, 0});
    const auto bytes = read_texture(*texture, {0, 0, {extent, extent, 1}, 4});
    expect_pixels(bytes, bytes_of<float>({0.375f, 0.375f, 0.375f, 0.375f}), 0);
}

TEST_F(CommandsTest, ClearsDepthOfDepthStencil) {
    const auto texture = make_texture(
        texture_desc(TextureType::tex2d, PixelFormat::depth32_float_stencil8, {extent, extent, 1}));
    const auto both = static_cast<ClearAspect>(0x3);
    recorder().clear_depth_stencil(*texture, whole, {0.625f, 0x5a, both, 0});
    const auto bytes = read_texture(*texture, {0, 0, {extent, extent, 1}, 4});
    expect_pixels(bytes, bytes_of<float>({0.625f, 0.625f, 0.625f, 0.625f}), 0);
}

TEST_F(CommandsTest, ClearsStencil) {
    const auto texture =
        make_texture(texture_desc(TextureType::tex2d, PixelFormat::stencil8, {extent, extent, 1}));
    recorder().clear_depth_stencil(*texture, whole, {0.0f, 0x5a, ClearAspect::stencil, 0});
    const auto bytes = read_texture(*texture, {0, 0, {extent, extent, 1}, 1});
    expect_pixels(bytes, {0x5a}, 0);
}

} // namespace
