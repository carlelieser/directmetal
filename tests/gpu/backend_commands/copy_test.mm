#include "commands_fixture.hpp"

namespace {

using namespace directmetal::backend;
using namespace directmetal::backend::testing;

constexpr std::uint8_t sentinel = 0xcd;

std::uint8_t pattern(std::size_t index) {
    return static_cast<std::uint8_t>((index * 7 + 3) & 0xff);
}

void fill_pattern(Buffer& buffer) {
    for (std::size_t index = 0; index < buffer.size(); ++index) {
        buffer.cpu_address()[index] = static_cast<std::byte>(pattern(index));
    }
}

std::uint8_t byte_at(Buffer& buffer, std::size_t offset) {
    return std::to_integer<std::uint8_t>(buffer.cpu_address()[offset]);
}

// Byte offset of a texel in a pitched layout.
struct Layout {
    std::uint64_t offset;
    std::uint32_t row_pitch;
    std::uint32_t slice_pitch;

    std::uint64_t at(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
        return offset + std::uint64_t{z} * slice_pitch + std::uint64_t{y} * row_pitch + x * 4;
    }
};

TEST_F(CommandsTest, CopiesBufferRange) {
    const auto source = make_buffer(4 * kib);
    const auto destination = make_filled_buffer(4 * kib, sentinel);
    fill_pattern(*source);
    recorder().copy_buffer({*source, 64, *destination, 512, 256});
    submit_and_wait();
    for (std::size_t index = 0; index < destination->size(); ++index) {
        const bool is_copied = index >= 512 && index < 768;
        const std::uint8_t expected = is_copied ? pattern(index - 512 + 64) : sentinel;
        ASSERT_EQ(byte_at(*destination, index), expected) << index;
    }
}

// Uploads with one set of pitches and reads back with another; padding stays untouched.
TEST_F(CommandsTest, CopiesBetweenBufferAndVolumeWithPitches) {
    const Extent3D extent{5, 3, 2};
    const Layout upload_layout{512, 256, 1024};
    const Layout readback_layout{16, 32, 128};
    const auto upload = make_buffer(4 * kib);
    const auto readback = make_filled_buffer(4 * kib, sentinel);
    fill_pattern(*upload);
    const auto texture =
        make_texture(texture_desc(TextureType::tex3d, PixelFormat::rgba8_uint, extent));
    recorder().copy_buffer_to_texture({*upload,
                                       upload_layout.offset,
                                       upload_layout.row_pitch,
                                       upload_layout.slice_pitch,
                                       *texture,
                                       0,
                                       0,
                                       {0, 0, 0},
                                       extent});
    recorder().copy_texture_to_buffer({*readback,
                                       readback_layout.offset,
                                       readback_layout.row_pitch,
                                       readback_layout.slice_pitch,
                                       *texture,
                                       0,
                                       0,
                                       {0, 0, 0},
                                       extent});
    submit_and_wait();
    std::vector<std::uint8_t> expected(readback->size(), sentinel);
    for (std::uint32_t z = 0; z < extent.depth; ++z) {
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t byte = 0; byte < extent.width * 4; ++byte) {
                expected[readback_layout.at(0, y, z) + byte] =
                    pattern(upload_layout.at(0, y, z) + byte);
            }
        }
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        ASSERT_EQ(byte_at(*readback, index), expected[index]) << index;
    }
}

TEST_F(CommandsTest, CopiesBufferIntoSubresourceRegion) {
    TextureDesc desc = texture_desc(TextureType::tex2d_array, PixelFormat::rgba8_uint, {8, 8, 1});
    desc.mip_levels = 2;
    desc.array_length = 2;
    const auto texture = make_texture(desc);
    recorder().clear_texture(*texture, {0, 2, 0, 2}, {{1, 2, 3, 4}, ClearKind::uint_value, {}});
    const auto upload = make_buffer(4 * kib);
    fill_pattern(*upload);
    const Layout layout{0, 64, 128};
    recorder().copy_buffer_to_texture(
        {*upload, 0, layout.row_pitch, layout.slice_pitch, *texture, 1, 1, {1, 2, 0}, {3, 2, 1}});
    const auto bytes = read_texture(*texture, {1, 1, {4, 4, 1}, 4});
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const bool is_copied = x >= 1 && y >= 2;
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const std::uint8_t expected =
                    is_copied ? pattern(layout.at(x - 1, y - 2, 0) + channel) : channel + 1;
                ASSERT_EQ(std::to_integer<std::uint8_t>(bytes[(y * 4 + x) * 4 + channel]), expected)
                    << x << "," << y;
            }
        }
    }
}

TEST_F(CommandsTest, CopiesTextureRegionBetweenSubresources) {
    const auto source =
        make_texture(texture_desc(TextureType::tex2d, PixelFormat::rgba8_uint, {8, 8, 1}));
    TextureDesc desc = texture_desc(TextureType::tex2d_array, PixelFormat::rgba8_uint, {16, 16, 1});
    desc.mip_levels = 2;
    desc.array_length = 2;
    const auto destination = make_texture(desc);
    const auto upload = make_buffer(256);
    fill_pattern(*upload);
    recorder().copy_buffer_to_texture({*upload, 0, 32, 256, *source, 0, 0, {0, 0, 0}, {8, 8, 1}});
    recorder().clear_texture(*destination, {0, 2, 0, 2}, {{9, 9, 9, 9}, ClearKind::uint_value, {}});
    recorder().copy_texture({*source, 0, 0, {2, 3, 0}, *destination, 1, 1, {1, 2, 0}, {4, 3, 1}});
    const auto bytes = read_texture(*destination, {1, 1, {8, 8, 1}, 4});
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const bool is_inside_columns = x >= 1 && x < 5;
            const bool is_copied = is_inside_columns && y >= 2 && y < 5;
            const std::size_t source_byte = ((y - 2 + 3) * 8 + (x - 1 + 2)) * 4;
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const std::uint8_t expected = is_copied ? pattern(source_byte + channel) : 9;
                ASSERT_EQ(std::to_integer<std::uint8_t>(bytes[(y * 8 + x) * 4 + channel]), expected)
                    << x << "," << y;
            }
        }
    }
}

} // namespace
