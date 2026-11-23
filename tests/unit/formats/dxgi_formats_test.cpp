#include <directmetal/formats/formats.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace {

using directmetal::backend::GpuFamily;
using directmetal::backend::PixelFormat;
using namespace directmetal::formats;

constexpr std::array all_families = {GpuFamily::apple7, GpuFamily::apple8, GpuFamily::apple9,
                                     GpuFamily::apple10};

bool has(Support support, Support flags) {
    return (support & flags) == flags;
}

bool has_any(Support support, Support flags) {
    return (support & flags) != Support::none;
}

const FormatInfo& info(DxgiFormat format) {
    const FormatInfo* found = find(format);
    EXPECT_NE(found, nullptr) << "format " << format;
    static const FormatInfo missing{};
    return found == nullptr ? missing : *found;
}

TEST(DxgiFormats, ListsEveryEnumeratorInEnumOrder) {
    const auto formats = all_dxgi_formats();
    ASSERT_EQ(formats.size(), 122u);
    EXPECT_EQ(formats.front().name, "DXGI_FORMAT_UNKNOWN");
    EXPECT_EQ(formats.back().format, 191u);
    EXPECT_EQ(formats.back().name, "DXGI_FORMAT_A4B4G4R4_UNORM");
    const auto is_out_of_order = [](const FormatInfo& left, const FormatInfo& right) {
        return left.format >= right.format;
    };
    EXPECT_EQ(std::adjacent_find(formats.begin(), formats.end(), is_out_of_order), formats.end());
}

TEST(DxgiFormats, FindReturnsTheEntryForEveryEnumerator) {
    for (const FormatInfo& format : all_dxgi_formats()) {
        const FormatInfo* found = find(format.format);
        ASSERT_NE(found, nullptr) << format.name;
        EXPECT_EQ(found->name, format.name);
    }
}

TEST(DxgiFormats, FindRejectsValuesOutsideTheEnum) {
    for (const DxgiFormat value : {116u, 129u, 133u, 188u, 192u, 0xffffffffu}) {
        EXPECT_EQ(find(value), nullptr) << value;
        for (const GpuFamily family : all_families) {
            EXPECT_EQ(support(value, family), Support::none) << value;
        }
    }
}

TEST(DxgiFormats, TypedFormatsNameATypelessFamily) {
    for (const FormatInfo& format : all_dxgi_formats()) {
        if (format.kind == ComponentKind::typeless) {
            EXPECT_EQ(format.typeless_family, format.format) << format.name;
            continue;
        }
        if (format.typeless_family != 0) {
            const FormatInfo& family = info(format.typeless_family);
            EXPECT_EQ(family.kind, ComponentKind::typeless) << format.name;
            EXPECT_EQ(family.bytes_per_block, format.bytes_per_block) << format.name;
        }
    }
}

TEST(DxgiFormats, UnsupportedFormatsReportNoSupport) {
    for (const FormatInfo& format : all_dxgi_formats()) {
        const bool is_unsupported = format.substitution == Substitution::unsupported;
        EXPECT_EQ(is_unsupported, format.pixel_format == PixelFormat::invalid) << format.name;
        if (is_unsupported) {
            EXPECT_EQ(support(format.format, GpuFamily::apple10), Support::none) << format.name;
        }
    }
}

TEST(DxgiFormats, LaterFamiliesSupportEverythingEarlierOnesDo) {
    for (const FormatInfo& format : all_dxgi_formats()) {
        for (std::size_t index = 1; index < all_families.size(); ++index) {
            const Support earlier = support(format.format, all_families[index - 1]);
            const Support later = support(format.format, all_families[index]);
            EXPECT_TRUE(has(later, earlier)) << format.name;
        }
    }
}

TEST(DxgiFormats, IntegerFormatsAreNeitherFilteredNorBlended) {
    const Support filtered = Support::shader_sample | Support::blendable |
                             Support::multisample_resolve | Support::mip_autogen;
    for (const FormatInfo& format : all_dxgi_formats()) {
        const bool is_integer =
            format.kind == ComponentKind::uint || format.kind == ComponentKind::sint;
        if (is_integer) {
            EXPECT_FALSE(has_any(support(format.format, GpuFamily::apple10), filtered))
                << format.name;
        }
    }
}

TEST(DxgiFormats, ExitTestFormatsAreRenderableEverywhere) {
    struct Expected {
        DxgiFormat format;
        PixelFormat pixel_format;
    };
    constexpr Expected expected[] = {
        {28, PixelFormat::rgba8_unorm},  {29, PixelFormat::rgba8_unorm_srgb},
        {87, PixelFormat::bgra8_unorm},  {24, PixelFormat::rgb10a2_unorm},
        {10, PixelFormat::rgba16_float}, {2, PixelFormat::rgba32_float},
        {3, PixelFormat::rgba32_uint},
    };
    const Support required = Support::texture2d | Support::render_target | Support::shader_load;
    for (const Expected& entry : expected) {
        const FormatInfo& format = info(entry.format);
        EXPECT_EQ(format.pixel_format, entry.pixel_format) << format.name;
        EXPECT_EQ(format.substitution, Substitution::none) << format.name;
        for (const GpuFamily family : all_families) {
            EXPECT_TRUE(has(support(entry.format, family), required)) << format.name;
        }
    }
}

} // namespace
