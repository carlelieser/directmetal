#include "format_fixture.hpp"

#include "resource_access.hpp"

#include <algorithm>
#include <cstring>

namespace directmetal::formats::gpu_testing {
namespace {

struct Session {
    std::unique_ptr<backend::Device> device;
    std::unique_ptr<backend::Queue> queue;
    DeviceFeatures features;
};

std::unique_ptr<Session>& session() {
    static std::unique_ptr<Session> current;
    return current;
}

std::unique_ptr<Session> open_session() {
    auto device = backend::create_device({});
    if (!device.has_value()) {
        return nullptr;
    }
    auto queue = device.value()->create_queue();
    if (!queue.has_value()) {
        return nullptr;
    }
    const id<MTLDevice> metal = backend::resources::metal_device(*device.value());
    const DeviceFeatures features = device_features(device.value()->capabilities(), metal);
    return std::make_unique<Session>(
        Session{std::move(device).value(), std::move(queue).value(), features});
}

} // namespace

DeviceFeatures device_features(const backend::Capabilities& capabilities, id<MTLDevice> device) {
    return {capabilities.family, capabilities.has_bc != 0,
            static_cast<bool>(device.supports32BitFloatFiltering),
            device.readWriteTextureSupport == MTLReadWriteTextureTier2};
}

std::vector<const FormatInfo*> every_format() {
    std::vector<const FormatInfo*> formats;
    for (const FormatInfo& info : all_dxgi_formats()) {
        formats.push_back(&info);
    }
    return formats;
}

std::string format_test_name(const ::testing::TestParamInfo<const FormatInfo*>& info) {
    return short_name(*info.param);
}

void FormatGpuTest::SetUpTestSuite() {
    session() = open_session();
}

void FormatGpuTest::TearDownTestSuite() {
    session().reset();
}

void FormatGpuTest::SetUp() {
    ASSERT_NE(session(), nullptr) << "no Apple7 or later Metal device with a queue";
}

backend::Device& FormatGpuTest::device() {
    return *session()->device;
}

backend::Queue& FormatGpuTest::queue() {
    return *session()->queue;
}

id<MTLDevice> FormatGpuTest::metal_device() {
    return backend::resources::metal_device(device());
}

const DeviceFeatures& FormatGpuTest::features() {
    return session()->features;
}

void FormatGpuTest::submit_and_wait() {
    ASSERT_EQ(queue().submit(), backend::Status::success);
    ASSERT_EQ(queue().progress().wait(queue().last_submitted_value(), patience),
              backend::Status::success);
}

std::unique_ptr<backend::Buffer> FormatGpuTest::make_buffer(std::span<const std::byte> bytes) {
    auto buffer = device().create_buffer({bytes.size(), backend::StorageMode::shared, {}});
    EXPECT_TRUE(buffer.has_value()) << "create_buffer of " << bytes.size() << " bytes";
    if (!buffer.has_value()) {
        return nullptr;
    }
    std::memcpy(buffer.value()->cpu_address(), bytes.data(), bytes.size());
    return std::move(buffer).value();
}

std::unique_ptr<backend::Buffer> FormatGpuTest::make_filled_buffer(std::uint64_t size,
                                                                   std::byte fill) {
    const std::vector<std::byte> bytes(size, fill);
    return make_buffer(bytes);
}

std::unique_ptr<backend::Texture> FormatGpuTest::make_texture(const backend::TextureDesc& desc) {
    auto texture = device().create_texture(desc);
    EXPECT_TRUE(texture.has_value())
        << "create_texture failed with status " << static_cast<int>(texture.status());
    return texture.has_value() ? std::move(texture).value() : nullptr;
}

void FormatGpuTest::record_upload(backend::Buffer& source, backend::Texture& texture,
                                  const CopyLayout& layout) {
    for (std::uint32_t slice = 0; slice < layout.slice_count; ++slice) {
        queue().recorder().copy_buffer_to_texture({source,
                                                   slice * layout.slice_bytes(),
                                                   layout.row_pitch,
                                                   layout.image_pitch,
                                                   texture,
                                                   0,
                                                   slice,
                                                   {0, 0, 0},
                                                   layout.extent});
    }
}

void FormatGpuTest::record_readback(backend::Texture& texture, const CopyLayout& layout,
                                    backend::Buffer& destination) {
    for (std::uint32_t slice = 0; slice < layout.slice_count; ++slice) {
        queue().recorder().copy_texture_to_buffer({destination,
                                                   slice * layout.slice_bytes(),
                                                   layout.row_pitch,
                                                   layout.image_pitch,
                                                   texture,
                                                   0,
                                                   slice,
                                                   {0, 0, 0},
                                                   layout.extent});
    }
}

std::span<const std::byte> contents(backend::Buffer& buffer) {
    return {buffer.cpu_address(), static_cast<std::size_t>(buffer.size())};
}

std::size_t first_difference(std::span<const std::byte> actual,
                             std::span<const std::byte> expected) {
    const auto [actual_end, expected_end] = std::ranges::mismatch(actual, expected);
    return static_cast<std::size_t>(actual_end - actual.begin());
}

} // namespace directmetal::formats::gpu_testing
