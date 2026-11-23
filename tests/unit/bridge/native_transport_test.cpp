#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/device_client.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace {

using namespace directmetal::bridge;

bool is_zeroed(const std::byte* memory, std::uint64_t size) {
    return std::all_of(memory, memory + size, [](std::byte value) { return value == std::byte{}; });
}

TEST(NativeTransport, ReachesHostDispatchForQueryVersion) {
    QueryVersionParams params{};
    EXPECT_EQ(call(FunctionCode::query_version, &params), status_success);
    EXPECT_EQ(params.version, protocol_version);
}

TEST(NativeTransport, ReachesHostDispatchForLogCall) {
    constexpr std::string_view name = "NativeTransport";
    LogCallParams params{};
    params.name = to_address(name.data());
    params.name_length = static_cast<std::uint32_t>(name.size());
    EXPECT_EQ(call(FunctionCode::log_call, &params), status_success);
    EXPECT_STRNE(params.host_system, "");
}

// The host rejects null parameters for every control call.
TEST(NativeTransport, PassesNewControlCallsToHostDispatch) {
    for (auto code = to_index(FunctionCode::initialize); code < to_index(FunctionCode::count);
         ++code) {
        EXPECT_EQ(directmetal_bridge_call(code, nullptr), status_invalid_parameter)
            << "code " << code;
    }
}

TEST(NativeTransport, CarriesClientCallsToHostDispatch) {
    EXPECT_EQ(query_version().value(), protocol_version);
    // Creating a device would create a Metal device, which suite unit must not.
    EXPECT_EQ(initialize("native_transport_test", false), directmetal::backend::Status::success);
}

TEST(NativeTransport, RejectsUnknownCodes) {
    EXPECT_EQ(directmetal_bridge_call(to_index(FunctionCode::count), nullptr),
              status_invalid_parameter);
}

TEST(NativeTransport, AllocatesAlignedZeroedSharedMemory) {
    for (std::uint64_t size :
         {std::uint64_t{1}, shared_memory_alignment, 3 * shared_memory_alignment + 5}) {
        auto* memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(size));
        ASSERT_NE(memory, nullptr) << "size " << size;
        EXPECT_EQ(directmetal::bridge::to_address(memory) % shared_memory_alignment, 0u)
            << "size " << size;
        const std::uint64_t rounded = (size + shared_memory_alignment - 1) /
                                      shared_memory_alignment * shared_memory_alignment;
        EXPECT_TRUE(is_zeroed(memory, rounded)) << "size " << size;
        std::memset(memory, 0xA5, rounded);
        directmetal_bridge_free_shared(memory, size);
    }
}

TEST(NativeTransport, ReturnsNullForUnrepresentableSizes) {
    EXPECT_EQ(directmetal_bridge_allocate_shared(0), nullptr);
    EXPECT_EQ(directmetal_bridge_allocate_shared(UINT64_MAX), nullptr);
    EXPECT_EQ(directmetal_bridge_allocate_shared(UINT64_MAX - shared_memory_alignment), nullptr);
}

TEST(NativeTransport, IgnoresFreeOfNull) {
    directmetal_bridge_free_shared(nullptr, shared_memory_alignment);
}

} // namespace
