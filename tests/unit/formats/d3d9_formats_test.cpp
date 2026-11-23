#include <directmetal/formats/formats.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace {

using directmetal::backend::PixelFormat;
using namespace directmetal::formats;

bool has(D3d9Usage usage, D3d9Usage flags) {
    return (usage & flags) == flags;
}

constexpr std::uint32_t fourcc(std::string_view code) {
    return static_cast<std::uint32_t>(code[0]) | static_cast<std::uint32_t>(code[1]) << 8 |
           static_cast<std::uint32_t>(code[2]) << 16 | static_cast<std::uint32_t>(code[3]) << 24;
}

const D3d9FormatInfo& info(std::uint32_t value) {
    const D3d9FormatInfo* found = find_d3d9(value);
    EXPECT_NE(found, nullptr) << "D3DFORMAT " << value;
    static const D3d9FormatInfo missing{};
    return found == nullptr ? missing : *found;
}

TEST(D3d9Formats, ListsEveryFormatOnceSortedByValue) {
    const auto formats = all_d3d9_formats();
    EXPECT_EQ(formats.size(), 72u);
    const auto is_out_of_order = [](const D3d9FormatInfo& left, const D3d9FormatInfo& right) {
        return left.value >= right.value;
    };
    EXPECT_EQ(std::adjacent_find(formats.begin(), formats.end(), is_out_of_order), formats.end());
    for (const D3d9FormatInfo& format : formats) {
        const D3d9FormatInfo* found = find_d3d9(format.value);
        ASSERT_NE(found, nullptr) << format.name;
        EXPECT_EQ(found->name, format.name);
    }
}

TEST(D3d9Formats, FindRejectsUnknownValues) {
    for (const std::uint32_t value : {1u, 37u, 120u, 0xffffffffu, fourcc("DXT6")}) {
        EXPECT_EQ(find_d3d9(value), nullptr) << value;
    }
}

TEST(D3d9Formats, IncludesTheFourccExtensions) {
    const std::array<std::string_view, 7> extensions = {"INTZ", "DF24", "DF16", "RAWZ",
                                                        "ATI1", "ATI2", "NULL"};
    for (const std::string_view extension : extensions) {
        EXPECT_EQ(info(fourcc(extension)).name, extension);
    }
    EXPECT_EQ(fourcc("INTZ"), 0x5a544e49u);
    EXPECT_TRUE(has(info(fourcc("INTZ")).usage, D3d9Usage::texture | D3d9Usage::depth_stencil));
    EXPECT_EQ(info(fourcc("ATI2")).pixel_format, PixelFormat::bc5_rgunorm);
    EXPECT_EQ(info(fourcc("NULL")).usage, D3d9Usage::render_target);
}

TEST(D3d9Formats, ExtensionsReadChannelsLikeWindowsDrivers) {
    // ATI2 stores X in the second BC5 channel; INTZ returns depth in every channel.
    const std::array<Swizzle, 4> ati2 = {Swizzle::green, Swizzle::red, Swizzle::one, Swizzle::one};
    const std::array<Swizzle, 4> intz = {Swizzle::red, Swizzle::red, Swizzle::red, Swizzle::red};
    const std::array<Swizzle, 4> depth = {Swizzle::red, Swizzle::zero, Swizzle::zero, Swizzle::one};
    EXPECT_EQ(info(fourcc("ATI2")).swizzle, ati2);
    EXPECT_EQ(info(fourcc("INTZ")).swizzle, intz);
    EXPECT_EQ(info(fourcc("DF24")).swizzle, depth);
    EXPECT_EQ(info(fourcc("DF16")).swizzle, depth);
}

TEST(D3d9Formats, MapsLockableStencilAndFourBitStencil) {
    EXPECT_EQ(info(85).pixel_format, PixelFormat::stencil8);
    EXPECT_TRUE(has(info(85).usage, D3d9Usage::depth_stencil));
    EXPECT_EQ(info(79).pixel_format, PixelFormat::depth32_float_stencil8);
    EXPECT_EQ(info(79).substitution, Substitution::depth24_to_depth32_stencil8);
    EXPECT_TRUE(has(info(79).usage, D3d9Usage::depth_stencil));
}

TEST(D3d9Formats, UnsupportedFormatsAdvertiseNoUsage) {
    for (const D3d9FormatInfo& format : all_d3d9_formats()) {
        if (format.substitution == Substitution::unsupported) {
            EXPECT_EQ(format.usage, D3d9Usage::none) << format.name;
            EXPECT_EQ(format.pixel_format, PixelFormat::invalid) << format.name;
        }
        if (format.usage != D3d9Usage::none && format.value != fourcc("NULL")) {
            EXPECT_NE(format.pixel_format, PixelFormat::invalid) << format.name;
        }
    }
}

TEST(D3d9Formats, MapsPackedFormatsFromTheMostSignificantBits) {
    struct Expected {
        std::uint32_t value;
        PixelFormat pixel_format;
    };
    constexpr Expected expected[] = {
        {21, PixelFormat::bgra8_unorm},   {32, PixelFormat::rgba8_unorm},
        {23, PixelFormat::b5g6r5_unorm},  {25, PixelFormat::bgr5a1_unorm},
        {31, PixelFormat::rgb10a2_unorm}, {35, PixelFormat::bgr10a2_unorm},
        {34, PixelFormat::rg16_unorm},    {113, PixelFormat::rgba16_float},
    };
    for (const Expected& entry : expected) {
        EXPECT_EQ(info(entry.value).pixel_format, entry.pixel_format) << info(entry.value).name;
    }
}

TEST(D3d9Formats, SubstitutesDepthAndOpaqueFormats) {
    for (const std::uint32_t value : {75u, 77u, 83u, fourcc("INTZ")}) {
        EXPECT_EQ(info(value).pixel_format, PixelFormat::depth32_float_stencil8) << value;
        EXPECT_EQ(info(value).substitution, Substitution::depth24_to_depth32_stencil8) << value;
        EXPECT_TRUE(has(info(value).usage, D3d9Usage::depth_stencil)) << value;
    }
    const std::array<Swizzle, 4> alpha_one = {Swizzle::red, Swizzle::green, Swizzle::blue,
                                              Swizzle::one};
    EXPECT_EQ(info(22).substitution, Substitution::alpha_forced_one);
    EXPECT_EQ(info(22).swizzle, alpha_one);
    EXPECT_TRUE(has(info(22).usage, D3d9Usage::render_target | D3d9Usage::srgb_write));
}

TEST(D3d9Formats, ReplicatesLuminanceAndFillsMissingChannelsWithOne) {
    const std::array<Swizzle, 4> luminance = {Swizzle::red, Swizzle::red, Swizzle::red,
                                              Swizzle::one};
    const std::array<Swizzle, 4> luminance_alpha = {Swizzle::red, Swizzle::red, Swizzle::red,
                                                    Swizzle::green};
    const std::array<Swizzle, 4> red_only = {Swizzle::red, Swizzle::one, Swizzle::one,
                                             Swizzle::one};
    EXPECT_EQ(info(50).swizzle, luminance);
    EXPECT_EQ(info(51).swizzle, luminance_alpha);
    EXPECT_EQ(info(111).swizzle, red_only);
    EXPECT_EQ(info(52).substitution, Substitution::emulated_unpack);
}

TEST(D3d9Formats, CompressedFormatsUseFourByFourBlocks) {
    struct Expected {
        std::string_view code;
        PixelFormat pixel_format;
        std::uint8_t bytes;
    };
    constexpr Expected expected[] = {
        {"DXT1", PixelFormat::bc1_rgba, 8},  {"DXT2", PixelFormat::bc2_rgba, 16},
        {"DXT3", PixelFormat::bc2_rgba, 16}, {"DXT4", PixelFormat::bc3_rgba, 16},
        {"DXT5", PixelFormat::bc3_rgba, 16}, {"ATI1", PixelFormat::bc4_runorm, 8},
    };
    for (const Expected& entry : expected) {
        const D3d9FormatInfo& format = info(fourcc(entry.code));
        EXPECT_EQ(format.pixel_format, entry.pixel_format) << entry.code;
        EXPECT_EQ(format.bytes_per_block, entry.bytes) << entry.code;
        EXPECT_EQ(format.block_width, 4) << entry.code;
        EXPECT_EQ(format.block_height, 4) << entry.code;
    }
}

} // namespace
