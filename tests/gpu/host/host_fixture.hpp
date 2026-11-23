#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/bridge/stream_writer.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <vector>

namespace directmetal::backend {

inline void PrintTo(Status status, std::ostream* out) {
    *out << "backend::Status " << static_cast<int>(status);
}

} // namespace directmetal::backend

// Drives the process-wide host on the Metal backend through the native in-process bridge, as a
// front end would: control calls through DeviceClient and commands through StreamWriter.
namespace directmetal::host::gpu_testing {

// Long enough for any test's GPU work; a broken wait returns timeout instead of hanging.
inline constexpr std::chrono::nanoseconds patience = std::chrono::seconds(10);

// Shared memory imported as a buffer, released and freed by the fixture unless a test released it.
struct ImportedRegion {
    std::span<std::byte> memory;
    bridge::Handle buffer;
    bool is_released;
};

// A whole mip of one slice, copied tightly packed into a fresh imported region.
struct Readback {
    bridge::Handle texture;
    std::uint32_t mip;
    backend::Extent3D extent;
    std::uint32_t bytes_per_pixel;
};

backend::TextureDesc texture_desc(backend::TextureType type, backend::PixelFormat format,
                                  backend::Extent3D extent);
backend::ClearColor float_color(float red, float green, float blue, float alpha);
backend::ClearColor uint_color(std::uint32_t red, std::uint32_t green, std::uint32_t blue,
                               std::uint32_t alpha);
backend::SubresourceRange mip_range(std::uint32_t mip);
bridge::BufferTextureRegion whole_mip_region(const Readback& readback, bridge::Handle buffer);

class HostGpuTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    bridge::DeviceClient& device() { return *device_; }
    bridge::StreamWriter& writer() { return *writer_; }
    bridge::StreamControl& control() { return *control_; }
    bridge::Handle stream() const { return stream_.stream; }

    bridge::Handle make_texture(const backend::TextureDesc& desc);
    // size is rounded up to shared_memory_alignment.
    ImportedRegion& import_region(std::uint64_t size);
    void clear(bridge::Handle texture, backend::SubresourceRange range, backend::ClearColor color);
    void clear_depth(bridge::Handle texture, backend::DepthStencilClear clear);
    void copy_whole_texture(bridge::Handle source, bridge::Handle destination,
                            backend::Extent3D extent);
    // Records the copy and returns the region it lands in; the bytes are valid after a wait.
    ImportedRegion& record_readback(const Readback& readback);
    std::vector<std::byte> read_back(const Readback& readback);
    // Records a submit and returns the progress value it signals.
    std::uint64_t submit();
    void submit_and_wait();
    backend::Status wait_progress(std::uint64_t value);
    bridge::Status stream_status() const;
    void close_stream();
    // Destroys the device, which also releases every import; the memory stays readable.
    void destroy_device();

private:
    std::optional<bridge::DeviceClient> device_;
    bridge::StreamControl* control_ = nullptr;
    bridge::OpenedStream stream_{};
    std::unique_ptr<bridge::StreamWriter> writer_;
    std::vector<std::unique_ptr<ImportedRegion>> regions_;
    std::uint64_t submitted_ = 0;
    bool is_stream_open_ = false;
};

} // namespace directmetal::host::gpu_testing
