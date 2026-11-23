#include "fake_transport.hpp"

#include <directmetal/bridge/device_client.hpp>
#include <directmetal/runtime/log.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;
namespace backend = directmetal::backend;
namespace log = directmetal::log;

constexpr Handle device_handle{0x100};

backend::Capabilities adapter_named(const char* name) {
    backend::Capabilities adapter{};
    std::strncpy(adapter.name.data(), name, adapter.name.size() - 1);
    return adapter;
}

// Answers create_device with device_handle and every later call with reply, recording each call.
class DeviceClientTest : public testing::Test {
protected:
    void SetUp() override {
        set_call_handler([this](FunctionCode code, void* params) { return on_call(code, params); });
    }
    void TearDown() override { set_call_handler({}); }

    Status on_call(FunctionCode code, void* params) {
        codes.push_back(code);
        last_params = params;
        if (code == FunctionCode::create_device) {
            auto& create = *static_cast<CreateDeviceParams*>(params);
            created = create;
            create.device = device_handle;
            create.capabilities = adapter_named("Fake GPU");
            return status_success;
        }
        return handler ? handler(code, params) : reply;
    }

    DeviceClient device() {
        auto result = DeviceClient::create(7, {1 << 20, 1 << 16});
        EXPECT_TRUE(result.has_value());
        return std::move(result).value();
    }

    std::vector<FunctionCode> codes;
    void* last_params = nullptr;
    CreateDeviceParams created{};
    Status reply = status_success;
    CallHandler handler;
};

TEST_F(DeviceClientTest, InitializePassesNameFlagsAndLogLevel) {
    InitializeParams seen{};
    std::string name;
    handler = [&](FunctionCode, void* params) {
        seen = *static_cast<InitializeParams*>(params);
        name.assign(from_address<const char>(seen.client_name), seen.client_name_length);
        return status_success;
    };
    log::set_level(log::Component::host, log::Level::debug);
    EXPECT_EQ(initialize("game.exe", true), backend::Status::success);
    log::set_level(log::Component::host, log::Level::info);
    EXPECT_EQ(codes, std::vector{FunctionCode::initialize});
    EXPECT_EQ(name, "game.exe");
    EXPECT_EQ(seen.flags, initialize_flag_wow64);
    EXPECT_EQ(seen.log_level, static_cast<std::uint32_t>(log::Level::debug));
    EXPECT_EQ(seen.reserved, 0u);

    EXPECT_EQ(initialize(std::string(max_name_length + 5, 'n'), false), backend::Status::success);
    EXPECT_EQ(seen.client_name_length, max_name_length);
    EXPECT_EQ(seen.flags, 0u);
}

TEST_F(DeviceClientTest, QueryVersionReturnsTheHostVersion) {
    handler = [](FunctionCode, void* params) {
        static_cast<QueryVersionParams*>(params)->version = 9;
        return status_success;
    };
    const auto version = query_version();
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version.value(), 9u);
}

TEST_F(DeviceClientTest, EnumerateAdaptersStopsAtNoMoreEntries) {
    handler = [](FunctionCode, void* params) {
        auto& enumerate = *static_cast<EnumerateAdaptersParams*>(params);
        if (enumerate.index >= 2) {
            return status_no_more_entries;
        }
        enumerate.adapter = adapter_named(enumerate.index == 0 ? "First" : "Second");
        return status_success;
    };
    const auto adapters = enumerate_adapters();
    ASSERT_TRUE(adapters.has_value());
    ASSERT_EQ(adapters.value().size(), 2u);
    EXPECT_STREQ(adapters.value()[0].name.data(), "First");
    EXPECT_STREQ(adapters.value()[1].name.data(), "Second");
    EXPECT_EQ(codes.size(), 3u);
}

TEST_F(DeviceClientTest, EnumerateAdaptersReportsFailures) {
    reply = status_device_removed;
    EXPECT_EQ(enumerate_adapters().status(), backend::Status::device_lost);
}

TEST_F(DeviceClientTest, CreateReturnsHandleAndCapabilities) {
    const DeviceClient client = device();
    EXPECT_EQ(client.handle().value, device_handle.value);
    EXPECT_STREQ(client.capabilities().name.data(), "Fake GPU");
    EXPECT_EQ(created.registry_id, 7u);
    EXPECT_EQ(created.options.heap_block_size, 1u << 20);
    EXPECT_EQ(created.options.dedicated_threshold, 1u << 16);
}

TEST_F(DeviceClientTest, CreateBufferAndTexturePassTheDevice) {
    DeviceClient client = device();
    handler = [](FunctionCode code, void* params) {
        if (code == FunctionCode::create_buffer) {
            auto& create = *static_cast<CreateBufferParams*>(params);
            create.buffer = {create.device.value + create.desc.size};
        } else {
            auto& create = *static_cast<CreateTextureParams*>(params);
            create.texture = {create.device.value + create.desc.extent.width};
        }
        return status_success;
    };
    const backend::BufferDesc buffer_desc{64, backend::StorageMode::shared, {}};
    EXPECT_EQ(client.create_buffer(buffer_desc).value().value, device_handle.value + 64);
    backend::TextureDesc texture_desc{};
    texture_desc.extent = {8, 8, 1};
    EXPECT_EQ(client.create_texture(texture_desc).value().value, device_handle.value + 8);
}

TEST_F(DeviceClientTest, ImportMemoryPassesAddressAndSize) {
    DeviceClient client = device();
    alignas(16) std::array<std::byte, 32> memory{};
    ImportMemoryParams seen{};
    handler = [&](FunctionCode, void* params) {
        auto& import = *static_cast<ImportMemoryParams*>(params);
        import.buffer = {0x200};
        import.is_zero_copy = 1;
        seen = import;
        return status_success;
    };
    const auto imported = client.import_memory(memory);
    ASSERT_TRUE(imported.has_value());
    EXPECT_EQ(imported.value().buffer.value, 0x200u);
    EXPECT_TRUE(imported.value().is_zero_copy);
    EXPECT_EQ(seen.address, reinterpret_cast<std::uintptr_t>(memory.data()));
    EXPECT_EQ(seen.size, memory.size());
}

TEST_F(DeviceClientTest, CreateTimelineAndOpenStreamPassSharedAddresses) {
    DeviceClient client = device();
    std::uint64_t mirror = 0;
    StreamControl control{};
    handler = [&](FunctionCode code, void* params) {
        if (code == FunctionCode::create_timeline) {
            auto& create = *static_cast<CreateTimelineParams*>(params);
            EXPECT_EQ(create.mirror_address, reinterpret_cast<std::uintptr_t>(&mirror));
            EXPECT_EQ(create.initial_value, 3u);
            create.timeline = {0x300};
        } else {
            auto& open = *static_cast<OpenStreamParams*>(params);
            EXPECT_EQ(open.control_address, reinterpret_cast<std::uintptr_t>(&control));
            open.stream = {0x400};
            open.progress_timeline = {0x401};
        }
        return status_success;
    };
    EXPECT_EQ(client.create_timeline(3, mirror).value().value, 0x300u);
    const auto stream = client.open_stream(control);
    ASSERT_TRUE(stream.has_value());
    EXPECT_EQ(stream.value().stream.value, 0x400u);
    EXPECT_EQ(stream.value().progress_timeline.value, 0x401u);
}

TEST_F(DeviceClientTest, TeardownCallsPassTheirHandles) {
    DeviceClient client = device();
    std::vector<std::uint64_t> handles;
    handler = [&](FunctionCode, void* params) {
        handles.push_back(static_cast<Handle*>(params)->value);
        return status_success;
    };
    client.destroy_timeline({1});
    client.close_stream({2});
    EXPECT_EQ(client.release_memory({3}), backend::Status::success);
    client.destroy();
    client.destroy();
    EXPECT_EQ(handles, (std::vector<std::uint64_t>{1, 2, 3, device_handle.value}));
    EXPECT_EQ(codes.back(), FunctionCode::destroy_device);
    EXPECT_TRUE(is_null(client.handle()));
}

} // namespace
