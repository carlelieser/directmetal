#include "host_fixture.hpp"

#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/shared_block_allocator.hpp>

#include <cstring>

namespace {

using namespace directmetal;
using namespace directmetal::host::gpu_testing;

TEST(HostSession, EnumeratesTheMetalAdapter) {
    ASSERT_EQ(bridge::initialize("host_gpu_tests", false), backend::Status::success);
    auto adapters = bridge::enumerate_adapters();
    ASSERT_TRUE(adapters.has_value()) << static_cast<int>(adapters.status());
    ASSERT_FALSE(adapters.value().empty());
    const backend::Capabilities& adapter = adapters.value().front();
    EXPECT_NE(adapter.registry_id, 0u);
    EXPECT_GT(std::strlen(adapter.name.data()), 0u);
    EXPECT_GE(adapter.max_texture_2d, 16384u);
    EXPECT_GT(adapter.max_buffer_length, 0u);
}

TEST(HostSession, CreatesTheDeviceOfAnEnumeratedAdapter) {
    ASSERT_EQ(bridge::initialize("host_gpu_tests", false), backend::Status::success);
    auto adapters = bridge::enumerate_adapters();
    ASSERT_TRUE(adapters.has_value() && !adapters.value().empty());
    const std::uint64_t registry_id = adapters.value().front().registry_id;
    auto device = bridge::DeviceClient::create(registry_id, {});
    ASSERT_TRUE(device.has_value()) << static_cast<int>(device.status());
    EXPECT_EQ(device.value().capabilities().registry_id, registry_id);
    EXPECT_EQ(std::strcmp(device.value().capabilities().name.data(),
                          adapters.value().front().name.data()),
              0);
    device.value().destroy();
}

TEST(HostSession, DestroyedDeviceRefusesCalls) {
    ASSERT_EQ(bridge::initialize("host_gpu_tests", false), backend::Status::success);
    auto device = bridge::DeviceClient::create(0, {});
    ASSERT_TRUE(device.has_value()) << static_cast<int>(device.status());
    const bridge::Handle handle = device.value().handle();
    device.value().destroy();
    bridge::CreateBufferParams params{handle, {4096, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(bridge::call(bridge::FunctionCode::create_buffer, &params),
              bridge::status_invalid_handle);
}

using HostImportTest = HostGpuTest;

TEST_F(HostImportTest, ImportsAlignedSharedMemoryWithoutCopying) {
    const std::uint64_t size = 2 * bridge::shared_memory_alignment;
    auto* memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(size));
    ASSERT_NE(memory, nullptr);
    auto imported = device().import_memory({memory, size});
    ASSERT_TRUE(imported.has_value()) << static_cast<int>(imported.status());
    EXPECT_TRUE(imported.value().is_zero_copy);
    EXPECT_EQ(device().release_memory(imported.value().buffer), backend::Status::success);
    directmetal_bridge_free_shared(memory, size);
}

TEST_F(HostImportTest, RefusesUnalignedImports) {
    const std::uint64_t size = 2 * bridge::shared_memory_alignment;
    auto* memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(size));
    ASSERT_NE(memory, nullptr);
    const std::uint64_t half = bridge::shared_memory_alignment / 2;
    EXPECT_EQ(device().import_memory({memory + half, bridge::shared_memory_alignment}).status(),
              backend::Status::invalid_argument);
    EXPECT_EQ(device().import_memory({memory, half}).status(), backend::Status::invalid_argument);
    directmetal_bridge_free_shared(memory, size);
}

using HostStreamTest = HostGpuTest;

TEST_F(HostStreamTest, MirrorsProgressOfEachSubmit) {
    const bridge::Handle texture = make_texture(
        texture_desc(backend::TextureType::tex2d, backend::PixelFormat::rgba8_unorm, {8, 8, 1}));
    for (std::uint64_t expected = 1; expected <= 3; ++expected) {
        clear(texture, mip_range(0), float_color(0.2f, 0.4f, 0.6f, 0.8f));
        const std::uint64_t value = submit();
        EXPECT_EQ(value, expected);
        ASSERT_EQ(wait_progress(value), backend::Status::success);
        EXPECT_GE(control().completed_progress, value);
    }
    EXPECT_EQ(control().retired_sequence, writer().sequence() - 1);
    EXPECT_EQ(stream_status(), bridge::status_success);
}

TEST_F(HostStreamTest, RefusesASecondStreamOnTheDevice) {
    bridge::StreamControl* second = bridge::shared_blocks().allocate_stream_control();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(device().open_stream(*second).status(), backend::Status::invalid_argument);
    close_stream();
    auto reopened = device().open_stream(*second);
    ASSERT_TRUE(reopened.has_value()) << static_cast<int>(reopened.status());
    device().close_stream(reopened.value().stream);
    bridge::shared_blocks().free(second, sizeof(bridge::StreamControl));
}

} // namespace
