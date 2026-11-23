#pragma once

#include <directmetal/backend/device.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace directmetal::backend::testing {

inline constexpr std::uint64_t kib = 1024;
inline constexpr std::uint64_t mib = 1024 * kib;
// Long enough for any test's GPU work; a broken release returns timeout instead of hanging.
inline constexpr std::chrono::nanoseconds patience = std::chrono::seconds(10);

struct ReadRegion {
    std::uint32_t mip;
    std::uint32_t slice;
    Extent3D extent;
    std::uint32_t bytes_per_pixel;
};

inline TextureDesc texture_desc(TextureType type, PixelFormat format, Extent3D extent) {
    return {type,
            StorageMode::gpu_private,
            0,
            format,
            extent,
            1,
            1,
            1,
            TextureUsage::shader_read | TextureUsage::render_target};
}

// A device and one queue on it, with helpers that record, submit and wait.
class CommandsTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto device = create_device({});
        ASSERT_TRUE(device.has_value()) << static_cast<int>(device.status());
        device_ = std::move(device).value();
        auto queue = device_->create_queue();
        ASSERT_TRUE(queue.has_value()) << static_cast<int>(queue.status());
        queue_ = std::move(queue).value();
    }

    void TearDown() override {
        queue_.reset();
        device_.reset();
    }

    CommandRecorder& recorder() { return queue_->recorder(); }

    void submit_and_wait() {
        ASSERT_EQ(queue_->submit(), Status::success);
        ASSERT_EQ(queue_->progress().wait(queue_->last_submitted_value(), patience),
                  Status::success);
    }

    std::unique_ptr<Buffer> make_buffer(std::uint64_t size) {
        auto buffer = device_->create_buffer({size, StorageMode::shared, {}});
        EXPECT_TRUE(buffer.has_value()) << static_cast<int>(buffer.status());
        return buffer.has_value() ? std::move(buffer).value() : nullptr;
    }

    std::unique_ptr<Buffer> make_filled_buffer(std::uint64_t size, std::uint8_t fill) {
        auto buffer = make_buffer(size);
        std::memset(buffer->cpu_address(), fill, size);
        return buffer;
    }

    std::unique_ptr<Texture> make_texture(const TextureDesc& desc) {
        auto texture = device_->create_texture(desc);
        EXPECT_TRUE(texture.has_value()) << static_cast<int>(texture.status());
        return texture.has_value() ? std::move(texture).value() : nullptr;
    }

    std::unique_ptr<Timeline> make_timeline(std::uint64_t initial_value) {
        auto timeline = device_->create_timeline(initial_value);
        EXPECT_TRUE(timeline.has_value()) << static_cast<int>(timeline.status());
        return timeline.has_value() ? std::move(timeline).value() : nullptr;
    }

    // Copies a subresource region into a tightly packed buffer and returns its bytes.
    std::vector<std::byte> read_texture(Texture& texture, const ReadRegion& region) {
        const std::uint32_t row_pitch = region.extent.width * region.bytes_per_pixel;
        const std::uint32_t slice_pitch = row_pitch * region.extent.height;
        const std::uint64_t size = std::uint64_t{slice_pitch} * region.extent.depth;
        const auto readback = make_filled_buffer(size, 0);
        recorder().copy_texture_to_buffer({*readback,
                                           0,
                                           row_pitch,
                                           slice_pitch,
                                           texture,
                                           region.mip,
                                           region.slice,
                                           {0, 0, 0},
                                           region.extent});
        submit_and_wait();
        const std::byte* bytes = readback->cpu_address();
        return {bytes, bytes + size};
    }

    std::unique_ptr<Device> device_;
    std::unique_ptr<Queue> queue_;
};

} // namespace directmetal::backend::testing
