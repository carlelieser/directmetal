#include "fake_transport.hpp"

#include <directmetal/bridge/device_client.hpp>
#include <directmetal/bridge/staging_ring.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;
namespace backend = directmetal::backend;

constexpr Handle device_handle{0x100};
constexpr Handle buffer_handle{0x200};

// Answers create_device and import_memory, and release_memory with release_reply.
class StagingRingTest : public testing::Test {
protected:
    void SetUp() override {
        reset_shared_memory_stats();
        set_call_handler([this](FunctionCode code, void* params) { return on_call(code, params); });
        auto created = DeviceClient::create(0, {});
        ASSERT_TRUE(created.has_value());
        device_.emplace(std::move(created).value());
    }
    void TearDown() override {
        set_call_handler({});
        EXPECT_EQ(shared_memory_stats().mismatched_frees, 0u);
    }

    Status on_call(FunctionCode code, void* params) {
        if (code == FunctionCode::create_device) {
            static_cast<CreateDeviceParams*>(params)->device = device_handle;
        } else if (code == FunctionCode::import_memory) {
            auto& import = *static_cast<ImportMemoryParams*>(params);
            import.buffer = buffer_handle;
            import.is_zero_copy = 1;
        } else if (code == FunctionCode::release_memory) {
            ++releases;
            return release_reply;
        }
        return status_success;
    }

    backend::Result<StagingRing> create(std::uint64_t capacity) {
        return StagingRing::create(*device_, capacity);
    }

    int releases = 0;
    Status release_reply = status_success;

private:
    std::optional<DeviceClient> device_;
};

TEST_F(StagingRingTest, RejectsZeroCapacity) {
    EXPECT_EQ(create(0).status(), backend::Status::invalid_argument);
    EXPECT_EQ(shared_memory_stats().total, 0u);
}

TEST_F(StagingRingTest, RejectsCapacityThatOverflowsRounding) {
    EXPECT_EQ(create(UINT64_MAX).status(), backend::Status::invalid_argument);
    EXPECT_EQ(shared_memory_stats().total, 0u);
}

TEST_F(StagingRingTest, RoundsCapacityUpToTheImportAlignment) {
    auto ring = create(1);
    ASSERT_TRUE(ring.has_value());
    EXPECT_EQ(ring.value().capacity(), shared_memory_alignment);
}

TEST_F(StagingRingTest, AlignsOffsetsToAtLeastFourBytes) {
    auto ring = create(shared_memory_alignment);
    ASSERT_TRUE(ring.has_value());
    ASSERT_TRUE(ring.value().allocate(2, 1, 1).has_value());
    const auto second = ring.value().allocate(2, 1, 1);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->offset % 4, 0u);
}

TEST_F(StagingRingTest, AlignsCpuAddressesLikeOffsets) {
    auto ring = create(shared_memory_alignment);
    ASSERT_TRUE(ring.has_value());
    ASSERT_TRUE(ring.value().allocate(8, 4, 1).has_value());
    const auto aligned = ring.value().allocate(8, 256, 1);
    ASSERT_TRUE(aligned.has_value());
    EXPECT_EQ(aligned->offset, 256u);
    EXPECT_EQ(directmetal::bridge::to_address(aligned->cpu_address) % 256, 0u);
}

TEST_F(StagingRingTest, ReleaseFreesTheMemoryOnce) {
    auto ring = create(shared_memory_alignment);
    ASSERT_TRUE(ring.has_value());
    EXPECT_EQ(ring.value().release(), backend::Status::success);
    EXPECT_EQ(ring.value().release(), backend::Status::success);
    EXPECT_EQ(releases, 1);
    EXPECT_EQ(shared_memory_stats().live, 0u);
    EXPECT_EQ(ring.value().capacity(), 0u);
    EXPECT_FALSE(ring.value().allocate(4, 4, 1).has_value());
}

TEST_F(StagingRingTest, DestructionReleasesAndFrees) {
    {
        auto ring = create(shared_memory_alignment);
        ASSERT_TRUE(ring.has_value());
        EXPECT_EQ(shared_memory_stats().live, 1u);
    }
    EXPECT_EQ(releases, 1);
    EXPECT_EQ(shared_memory_stats().live, 0u);
}

TEST_F(StagingRingTest, KeepsTheMemoryWhenReleaseFails) {
    auto ring = create(shared_memory_alignment);
    ASSERT_TRUE(ring.has_value());
    release_reply = status_invalid_device_state;
    EXPECT_EQ(ring.value().release(), backend::Status::invalid_argument);
    EXPECT_EQ(shared_memory_stats().live, 1u);
    release_reply = status_success;
    EXPECT_EQ(ring.value().release(), backend::Status::success);
    EXPECT_EQ(releases, 2);
    EXPECT_EQ(shared_memory_stats().live, 0u);
}

TEST_F(StagingRingTest, MovedFromRingReleasesNothing) {
    auto ring = create(shared_memory_alignment);
    ASSERT_TRUE(ring.has_value());
    StagingRing moved(std::move(ring).value());
    EXPECT_EQ(releases, 0);
    EXPECT_EQ(moved.release(), backend::Status::success);
    EXPECT_EQ(releases, 1);
}

} // namespace
