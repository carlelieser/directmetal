#include "host_fixture.hpp"

#include <cmath>
#include <cstring>

// Texture kinds whose host rules were written against the Metal documentation: each is created,
// cleared and copied on the real backend, where Metal validation aborts on anything it rejects.
namespace {

using namespace directmetal;
using namespace directmetal::host::gpu_testing;
using backend::PixelFormat;
using backend::StorageMode;
using backend::TextureType;

constexpr backend::Extent3D size_8x8{8, 8, 1};
constexpr backend::Extent3D line_16{16, 1, 1};
constexpr backend::DepthStencilClear depth_quarter{0.25f, 0, backend::ClearAspect::depth, 0};

std::vector<float> as_floats(const std::vector<std::byte>& bytes) {
    std::vector<float> values(bytes.size() / sizeof(float));
    std::memcpy(values.data(), bytes.data(), values.size() * sizeof(float));
    return values;
}

std::vector<std::uint16_t> as_halfwords(const std::vector<std::byte>& bytes) {
    std::vector<std::uint16_t> values(bytes.size() / sizeof(std::uint16_t));
    std::memcpy(values.data(), bytes.data(), values.size() * sizeof(std::uint16_t));
    return values;
}

void expect_all_equal(const std::vector<float>& values, float expected) {
    ASSERT_FALSE(values.empty());
    for (std::size_t index = 0; index < values.size(); ++index) {
        ASSERT_EQ(values[index], expected) << "texel " << index;
    }
}

backend::TextureDesc depth_desc(PixelFormat format, StorageMode storage) {
    backend::TextureDesc desc = texture_desc(TextureType::tex2d, format, size_8x8);
    desc.storage = storage;
    return desc;
}

backend::TextureDesc multisample_desc(StorageMode storage) {
    backend::TextureDesc desc =
        texture_desc(TextureType::tex2d_ms, PixelFormat::rgba8_unorm, size_8x8);
    desc.storage = storage;
    desc.sample_count = 4;
    desc.usage = backend::TextureUsage::render_target;
    return desc;
}

backend::TextureDesc one_dimensional_desc(TextureType type, std::uint32_t array_length) {
    backend::TextureDesc desc = texture_desc(type, PixelFormat::rgba32_float, line_16);
    desc.usage = backend::TextureUsage::shader_read;
    desc.array_length = array_length;
    return desc;
}

class OneDimensionalTextureTest : public HostGpuTest {
protected:
    // Fills one slice of an rgba32_float line with value.
    void upload_floats(bridge::Handle texture, std::uint32_t slice, float value) {
        const ImportedRegion& upload = import_region(16 * 16);
        for (std::size_t channel = 0; channel < 16 * 4; ++channel) {
            std::memcpy(upload.memory.data() + channel * sizeof(float), &value, sizeof(float));
        }
        bridge::BufferTextureRegion copy =
            whole_mip_region({texture, 0, line_16, 16}, upload.buffer);
        copy.slice = slice;
        writer().record(bridge::CopyBufferToTextureCommand{copy});
    }
};

// Metal cannot render to 1D textures: the host refuses them as render targets before Metal sees
// one, so they are filled through buffer copies.
TEST_F(OneDimensionalTextureTest, RefusesRenderTargets) {
    backend::TextureDesc desc = one_dimensional_desc(TextureType::tex1d, 1);
    desc.usage = backend::TextureUsage::shader_read | backend::TextureUsage::render_target;
    EXPECT_EQ(device().create_texture(desc).status(), backend::Status::invalid_argument);
    desc.type = TextureType::tex1d_array;
    desc.array_length = 2;
    EXPECT_EQ(device().create_texture(desc).status(), backend::Status::invalid_argument);
}

TEST_F(OneDimensionalTextureTest, UploadsAndReadsBackOne) {
    const bridge::Handle texture = make_texture(one_dimensional_desc(TextureType::tex1d, 1));
    upload_floats(texture, 0, 0.5f);
    expect_all_equal(as_floats(read_back({texture, 0, line_16, 16})), 0.5f);
}

TEST_F(OneDimensionalTextureTest, UploadsAndReadsBackOneSliceOfAnArray) {
    const bridge::Handle texture = make_texture(one_dimensional_desc(TextureType::tex1d_array, 2));
    upload_floats(texture, 0, 0.25f);
    upload_floats(texture, 1, 0.75f);
    const ImportedRegion& region = import_region(16 * 16);
    bridge::BufferTextureRegion copy = whole_mip_region({texture, 0, line_16, 16}, region.buffer);
    copy.slice = 1;
    writer().record(bridge::CopyTextureToBufferCommand{copy});
    submit_and_wait();
    const std::vector<std::byte> bytes(region.memory.begin(), region.memory.begin() + 16 * 16);
    expect_all_equal(as_floats(bytes), 0.75f);
}

TEST_F(OneDimensionalTextureTest, RefusesMipmappedOnes) {
    backend::TextureDesc desc =
        texture_desc(TextureType::tex1d, PixelFormat::rgba8_unorm, {16, 1, 1});
    desc.mip_levels = 2;
    EXPECT_EQ(device().create_texture(desc).status(), backend::Status::invalid_argument);
    desc.type = TextureType::tex1d_array;
    EXPECT_EQ(device().create_texture(desc).status(), backend::Status::invalid_argument);
}

class DepthTextureTest : public HostGpuTest, public ::testing::WithParamInterface<StorageMode> {};

TEST_P(DepthTextureTest, ReadsBackAClearedDepth32Float) {
    const bridge::Handle texture = make_texture(depth_desc(PixelFormat::depth32_float, GetParam()));
    clear_depth(texture, depth_quarter);
    expect_all_equal(as_floats(read_back({texture, 0, size_8x8, 4})), 0.25f);
}

TEST_P(DepthTextureTest, ReadsBackAClearedDepth16Unorm) {
    const bridge::Handle texture = make_texture(depth_desc(PixelFormat::depth16_unorm, GetParam()));
    clear_depth(texture, {0.2f, 0, backend::ClearAspect::depth, 0});
    for (const std::uint16_t value : as_halfwords(read_back({texture, 0, size_8x8, 2}))) {
        ASSERT_LE(std::abs(int{value} - 13107), 1) << value;
    }
}

TEST_P(DepthTextureTest, CopiesAWholeDepthSubresource) {
    const bridge::Handle source = make_texture(depth_desc(PixelFormat::depth32_float, GetParam()));
    const bridge::Handle destination =
        make_texture(depth_desc(PixelFormat::depth32_float, GetParam()));
    clear_depth(source, depth_quarter);
    clear_depth(destination, {1.0f, 0, backend::ClearAspect::depth, 0});
    copy_whole_texture(source, destination, size_8x8);
    expect_all_equal(as_floats(read_back({destination, 0, size_8x8, 4})), 0.25f);
}

TEST_P(DepthTextureTest, UploadsADepth32FloatFromABuffer) {
    const bridge::Handle texture = make_texture(depth_desc(PixelFormat::depth32_float, GetParam()));
    const ImportedRegion& upload = import_region(8 * 8 * 4);
    const float value = 0.625f;
    for (std::size_t texel = 0; texel < 8 * 8; ++texel) {
        std::memcpy(upload.memory.data() + texel * sizeof(float), &value, sizeof(float));
    }
    writer().record(bridge::CopyBufferToTextureCommand{
        whole_mip_region({texture, 0, size_8x8, 4}, upload.buffer)});
    expect_all_equal(as_floats(read_back({texture, 0, size_8x8, 4})), value);
}

INSTANTIATE_TEST_SUITE_P(Storage, DepthTextureTest,
                         ::testing::Values(StorageMode::shared, StorageMode::gpu_private),
                         [](const auto& info) {
                             return info.param == StorageMode::shared ? "shared" : "gpu_private";
                         });

class MultisampleTextureTest : public HostGpuTest,
                               public ::testing::WithParamInterface<StorageMode> {};

// Multisample textures cannot be copied to buffers, so the copy is checked only for completing
// without a stream error or a validation failure.
TEST_P(MultisampleTextureTest, ClearsAndCopiesAWholeSubresource) {
    const bridge::Handle source = make_texture(multisample_desc(GetParam()));
    const bridge::Handle destination = make_texture(multisample_desc(GetParam()));
    clear(source, mip_range(0), float_color(0.2f, 0.4f, 0.6f, 0.8f));
    copy_whole_texture(source, destination, size_8x8);
    submit_and_wait();
}

INSTANTIATE_TEST_SUITE_P(Storage, MultisampleTextureTest,
                         ::testing::Values(StorageMode::shared, StorageMode::gpu_private),
                         [](const auto& info) {
                             return info.param == StorageMode::shared ? "shared" : "gpu_private";
                         });

using DepthStencilBufferCopyTest = HostGpuTest;

TEST_F(DepthStencilBufferCopyTest, ReadsBackAClearedStencil8) {
    const bridge::Handle texture =
        make_texture(depth_desc(PixelFormat::stencil8, StorageMode::gpu_private));
    clear_depth(texture, {0.0f, 0x5A, backend::ClearAspect::stencil, 0});
    for (const std::byte value : read_back({texture, 0, size_8x8, 1})) {
        ASSERT_EQ(std::to_integer<int>(value), 0x5A);
    }
}

// Copying a combined format would need an aspect the command cannot name, so the host refuses it
// before it reaches Metal.
TEST_F(DepthStencilBufferCopyTest, RefusesACombinedDepthStencilFormat) {
    const bridge::Handle texture =
        make_texture(depth_desc(PixelFormat::depth32_float_stencil8, StorageMode::gpu_private));
    record_readback({texture, 0, size_8x8, 8});
    EXPECT_EQ(wait_progress(submit()), backend::Status::device_lost);
    EXPECT_EQ(stream_status(), bridge::status_invalid_parameter);
}

} // namespace
