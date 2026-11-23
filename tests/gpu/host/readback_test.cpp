#include "host_fixture.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace {

using namespace directmetal;
using namespace directmetal::host::gpu_testing;
using backend::PixelFormat;
using backend::TextureType;

constexpr backend::Extent3D size_8x8{8, 8, 1};

// Expected bytes of one pixel; tolerance is per byte (8-bit UNORM and sRGB only).
struct FormatCase {
    std::string name;
    PixelFormat format;
    std::uint32_t bytes_per_pixel;
    backend::ClearColor color;
    std::vector<std::uint8_t> pixel;
    int tolerance;
};

void PrintTo(const FormatCase& format, std::ostream* out) {
    *out << format.name;
}

template <typename Word, std::size_t count>
std::vector<std::uint8_t> bytes_of(const std::array<Word, count>& words) {
    std::vector<std::uint8_t> bytes(sizeof(words));
    std::memcpy(bytes.data(), words.data(), sizeof(words));
    return bytes;
}

std::uint32_t float_bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

// R in the low 10 bits, then G, B and a 2-bit A.
constexpr std::uint32_t rgb10a2(std::uint32_t red, std::uint32_t green, std::uint32_t blue,
                                std::uint32_t alpha) {
    return red | green << 10 | blue << 20 | alpha << 30;
}

// Colors sit away from rounding boundaries; sRGB values are the encodings of the linear clear.
std::vector<FormatCase> format_cases() {
    return {
        {"rgba8_unorm",
         PixelFormat::rgba8_unorm,
         4,
         float_color(0.2f, 0.4f, 0.6f, 0.8f),
         {51, 102, 153, 204},
         1},
        {"rgba8_unorm_srgb",
         PixelFormat::rgba8_unorm_srgb,
         4,
         float_color(0.1f, 0.15f, 0.6f, 0.8f),
         {89, 108, 203, 204},
         1},
        {"bgra8_unorm",
         PixelFormat::bgra8_unorm,
         4,
         float_color(0.2f, 0.4f, 0.6f, 0.8f),
         {153, 102, 51, 204},
         1},
        {"rgb10a2_unorm", PixelFormat::rgb10a2_unorm, 4,
         float_color(256.0f / 1023, 512.0f / 1023, 768.0f / 1023, 2.0f / 3),
         bytes_of(std::array{rgb10a2(256, 512, 768, 2)}), 0},
        {"rgba16_float", PixelFormat::rgba16_float, 8, float_color(0.25f, 0.5f, 0.75f, 1.0f),
         bytes_of(std::array<std::uint16_t, 4>{0x3400, 0x3800, 0x3A00, 0x3C00}), 0},
        {"rgba32_float", PixelFormat::rgba32_float, 16, float_color(0.25f, -2.0f, 1.5f, 1.0f),
         bytes_of(
             std::array{float_bits(0.25f), float_bits(-2.0f), float_bits(1.5f), float_bits(1.0f)}),
         0},
        {"rgba32_uint", PixelFormat::rgba32_uint, 16, uint_color(1, 0x12345678, 0xFFFF0000, 7),
         bytes_of(std::array<std::uint32_t, 4>{1, 0x12345678, 0xFFFF0000, 7}), 0},
    };
}

void expect_every_pixel(const std::vector<std::byte>& bytes, const std::vector<std::uint8_t>& pixel,
                        int tolerance) {
    ASSERT_EQ(bytes.size() % pixel.size(), 0u);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const int actual = std::to_integer<int>(bytes[index]);
        const int expected = pixel[index % pixel.size()];
        ASSERT_LE(std::abs(actual - expected), tolerance)
            << "byte " << index << ": " << actual << " against " << expected;
    }
}

class ClearReadbackTest : public HostGpuTest, public ::testing::WithParamInterface<FormatCase> {};

TEST_P(ClearReadbackTest, ReadsBackTheClearColor) {
    const FormatCase& format = GetParam();
    const bridge::Handle texture =
        make_texture(texture_desc(TextureType::tex2d, format.format, size_8x8));
    clear(texture, mip_range(0), format.color);
    const std::vector<std::byte> bytes = read_back({texture, 0, size_8x8, format.bytes_per_pixel});
    expect_every_pixel(bytes, format.pixel, format.tolerance);
}

INSTANTIATE_TEST_SUITE_P(Formats, ClearReadbackTest, ::testing::ValuesIn(format_cases()),
                         [](const auto& info) { return info.param.name; });

using MipReadbackTest = HostGpuTest;

TEST_F(MipReadbackTest, ClearsEachMipSeparately) {
    backend::TextureDesc desc =
        texture_desc(TextureType::tex2d, PixelFormat::rgba8_unorm, size_8x8);
    desc.mip_levels = 4;
    const bridge::Handle texture = make_texture(desc);
    clear(texture, mip_range(0), float_color(0.2f, 0.4f, 0.6f, 0.8f));
    clear(texture, mip_range(1), float_color(0.8f, 0.6f, 0.4f, 0.2f));
    const ImportedRegion& base = record_readback({texture, 0, size_8x8, 4});
    const std::vector<std::byte> mip1 = read_back({texture, 1, {4, 4, 1}, 4});
    expect_every_pixel(mip1, {204, 153, 102, 51}, 1);
    const std::vector<std::byte> mip0(base.memory.begin(), base.memory.begin() + 8 * 8 * 4);
    expect_every_pixel(mip0, {51, 102, 153, 204}, 1);
}

TEST_F(MipReadbackTest, LeavesOtherMipsUntouched) {
    backend::TextureDesc desc =
        texture_desc(TextureType::tex2d, PixelFormat::rgba8_unorm, size_8x8);
    desc.mip_levels = 2;
    const bridge::Handle texture = make_texture(desc);
    clear(texture, {0, 2, 0, 1}, float_color(0.2f, 0.4f, 0.6f, 0.8f));
    clear(texture, mip_range(1), float_color(0.0f, 0.0f, 1.0f, 1.0f));
    const ImportedRegion& base = record_readback({texture, 0, size_8x8, 4});
    const std::vector<std::byte> mip1 = read_back({texture, 1, {4, 4, 1}, 4});
    expect_every_pixel(mip1, {0, 0, 255, 255}, 0);
    const std::vector<std::byte> mip0(base.memory.begin(), base.memory.begin() + 8 * 8 * 4);
    expect_every_pixel(mip0, {51, 102, 153, 204}, 1);
}

} // namespace
