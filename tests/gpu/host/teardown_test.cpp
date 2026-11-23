#include "host_fixture.hpp"

#include <directmetal/bridge/client.hpp>

#include <array>

// Teardown rules of control.hpp on the Metal backend: each call returns only after the GPU has
// finished the work it waited for, so the imported memory holds the copy without a wait.
namespace {

using namespace directmetal;
using namespace directmetal::host::gpu_testing;

constexpr backend::Extent3D size_8x8{8, 8, 1};
constexpr std::size_t readback_size = 8 * 8 * 4;

bool holds_rgba8(const ImportedRegion& region, std::array<std::uint8_t, 4> pixel) {
    for (std::size_t index = 0; index < readback_size; ++index) {
        if (std::to_integer<std::uint8_t>(region.memory[index]) != pixel[index % 4]) {
            return false;
        }
    }
    return true;
}

class HostTeardownTest : public HostGpuTest {
protected:
    // Records a clear and its readback without a submit.
    ImportedRegion& record_unsubmitted() {
        const bridge::Handle texture = make_texture(
            texture_desc(backend::TextureType::tex2d, backend::PixelFormat::rgba8_unorm, size_8x8));
        clear(texture, mip_range(0), float_color(0.0f, 0.0f, 1.0f, 1.0f));
        return record_readback({texture, 0, size_8x8, 4});
    }
};

TEST_F(HostTeardownTest, ReleaseMemoryRefusesUnsubmittedWork) {
    ImportedRegion& region = record_unsubmitted();
    ASSERT_EQ(writer().flush(), bridge::status_success);
    EXPECT_EQ(device().release_memory(region.buffer), backend::Status::invalid_argument);
    submit();
    ASSERT_EQ(device().release_memory(region.buffer), backend::Status::success);
    region.is_released = true;
    EXPECT_TRUE(holds_rgba8(region, {0, 0, 255, 255}));
    EXPECT_EQ(stream_status(), bridge::status_success);
}

TEST_F(HostTeardownTest, ReleasesMemoryWithNothingRecorded) {
    ImportedRegion& region = import_region(bridge::shared_memory_alignment);
    ASSERT_EQ(device().release_memory(region.buffer), backend::Status::success);
    region.is_released = true;
}

TEST_F(HostTeardownTest, NamingAReleasedBufferIsAStreamError) {
    const bridge::Handle texture = make_texture(
        texture_desc(backend::TextureType::tex2d, backend::PixelFormat::rgba8_unorm, size_8x8));
    ImportedRegion& region = import_region(readback_size);
    ASSERT_EQ(device().release_memory(region.buffer), backend::Status::success);
    region.is_released = true;
    writer().record(bridge::CopyTextureToBufferCommand{
        whole_mip_region({texture, 0, size_8x8, 4}, region.buffer)});
    EXPECT_EQ(wait_progress(submit()), backend::Status::device_lost);
    EXPECT_EQ(stream_status(), bridge::status_invalid_handle);
    EXPECT_EQ(control().failed_opcode,
              static_cast<std::uint32_t>(bridge::Opcode::copy_texture_to_buffer));
}

TEST_F(HostTeardownTest, CloseStreamCommitsAndCompletesRecordedWork) {
    const ImportedRegion& region = record_unsubmitted();
    const bridge::Handle closed = stream();
    close_stream();
    EXPECT_TRUE(holds_rgba8(region, {0, 0, 255, 255}));
    bridge::SubmitStreamParams params{closed, 0, 0, 0, 1};
    EXPECT_EQ(bridge::call(bridge::FunctionCode::submit_stream, &params),
              bridge::status_invalid_handle);
}

TEST_F(HostTeardownTest, DestroyDeviceCommitsAndCompletesRecordedWork) {
    const ImportedRegion& region = record_unsubmitted();
    const bridge::Handle buffer = region.buffer;
    destroy_device();
    EXPECT_TRUE(holds_rgba8(region, {0, 0, 255, 255}));
    bridge::ReleaseMemoryParams params{buffer};
    EXPECT_EQ(bridge::call(bridge::FunctionCode::release_memory, &params),
              bridge::status_invalid_handle);
}

} // namespace
