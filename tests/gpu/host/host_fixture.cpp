#include "host_fixture.hpp"

#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/shared_block_allocator.hpp>

#include <atomic>
#include <bit>
#include <cstring>

namespace directmetal::host::gpu_testing {

using namespace bridge;

backend::TextureDesc texture_desc(backend::TextureType type, backend::PixelFormat format,
                                  backend::Extent3D extent) {
    return {type,
            backend::StorageMode::gpu_private,
            0,
            format,
            extent,
            1,
            1,
            1,
            backend::TextureUsage::shader_read | backend::TextureUsage::render_target};
}

backend::ClearColor float_color(float red, float green, float blue, float alpha) {
    return {{std::bit_cast<std::uint32_t>(red), std::bit_cast<std::uint32_t>(green),
             std::bit_cast<std::uint32_t>(blue), std::bit_cast<std::uint32_t>(alpha)},
            backend::ClearKind::float_value,
            {}};
}

backend::ClearColor uint_color(std::uint32_t red, std::uint32_t green, std::uint32_t blue,
                               std::uint32_t alpha) {
    return {{red, green, blue, alpha}, backend::ClearKind::uint_value, {}};
}

backend::SubresourceRange mip_range(std::uint32_t mip) {
    return {mip, 1, 0, 1};
}

BufferTextureRegion whole_mip_region(const Readback& readback, Handle buffer) {
    const std::uint32_t row_pitch = readback.extent.width * readback.bytes_per_pixel;
    return {buffer,
            0,
            row_pitch,
            row_pitch * readback.extent.height,
            readback.texture,
            readback.mip,
            0,
            {0, 0, 0},
            readback.extent};
}

void HostGpuTest::SetUp() {
    ASSERT_EQ(initialize("host_gpu_tests", false), backend::Status::success);
    auto device = DeviceClient::create(0, {});
    ASSERT_TRUE(device.has_value()) << static_cast<int>(device.status());
    device_.emplace(std::move(device).value());
    control_ = shared_blocks().allocate_stream_control();
    ASSERT_NE(control_, nullptr);
    auto opened = device_->open_stream(*control_);
    ASSERT_TRUE(opened.has_value()) << static_cast<int>(opened.status());
    stream_ = opened.value();
    is_stream_open_ = true;
    writer_ = std::make_unique<StreamWriter>(stream_.stream, *control_);
}

// close_stream commits recorded work, so every import can then be released.
void HostGpuTest::TearDown() {
    if (device_.has_value()) {
        close_stream();
        for (const auto& region : regions_) {
            if (!region->is_released) {
                EXPECT_EQ(device_->release_memory(region->buffer), backend::Status::success);
            }
        }
        device_->destroy();
    }
    for (const auto& region : regions_) {
        directmetal_bridge_free_shared(region->memory.data(), region->memory.size());
    }
    writer_.reset();
    if (control_ != nullptr) {
        shared_blocks().free(control_, sizeof(StreamControl));
    }
}

Handle HostGpuTest::make_texture(const backend::TextureDesc& desc) {
    auto texture = device_->create_texture(desc);
    EXPECT_TRUE(texture.has_value()) << "create_texture: " << static_cast<int>(texture.status());
    return texture.has_value() ? texture.value() : Handle{};
}

ImportedRegion& HostGpuTest::import_region(std::uint64_t size) {
    const std::uint64_t rounded =
        (size + shared_memory_alignment - 1) / shared_memory_alignment * shared_memory_alignment;
    auto* memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(rounded));
    EXPECT_NE(memory, nullptr) << "allocate_shared " << rounded;
    std::span<std::byte> span{memory, memory == nullptr ? 0 : rounded};
    auto imported = device_->import_memory(span);
    EXPECT_TRUE(imported.has_value()) << "import_memory: " << static_cast<int>(imported.status());
    const Handle buffer = imported.has_value() ? imported.value().buffer : Handle{};
    // A failed import has nothing to release.
    regions_.push_back(std::make_unique<ImportedRegion>(span, buffer, !imported.has_value()));
    return *regions_.back();
}

void HostGpuTest::clear(Handle texture, backend::SubresourceRange range,
                        backend::ClearColor color) {
    writer_->record(ClearTextureCommand{texture, range, color, 0});
}

void HostGpuTest::clear_depth(Handle texture, backend::DepthStencilClear clear) {
    writer_->record(ClearDepthStencilCommand{texture, mip_range(0), clear});
}

void HostGpuTest::copy_whole_texture(Handle source, Handle destination, backend::Extent3D extent) {
    writer_->record(
        CopyTextureCommand{source, 0, 0, {0, 0, 0}, 0, destination, 0, 0, {0, 0, 0}, extent, {}});
}

ImportedRegion& HostGpuTest::record_readback(const Readback& readback) {
    const std::uint64_t size =
        std::uint64_t{readback.extent.width} * readback.extent.height * readback.bytes_per_pixel;
    ImportedRegion& region = import_region(size);
    writer_->record(CopyTextureToBufferCommand{whole_mip_region(readback, region.buffer)});
    return region;
}

std::vector<std::byte> HostGpuTest::read_back(const Readback& readback) {
    const ImportedRegion& region = record_readback(readback);
    submit_and_wait();
    const std::size_t size =
        std::size_t{readback.extent.width} * readback.extent.height * readback.bytes_per_pixel;
    return {region.memory.begin(), region.memory.begin() + static_cast<std::ptrdiff_t>(size)};
}

std::uint64_t HostGpuTest::submit() {
    writer_->record(SubmitCommand{0});
    return ++submitted_;
}

void HostGpuTest::submit_and_wait() {
    const std::uint64_t value = submit();
    ASSERT_EQ(wait_progress(value), backend::Status::success);
    ASSERT_EQ(stream_status(), status_success) << "failed opcode " << control_->failed_opcode;
}

backend::Status HostGpuTest::wait_progress(std::uint64_t value) {
    writer_->flush();
    return TimelineClient(stream_.progress_timeline, &control_->completed_progress)
        .wait(value, patience);
}

Status HostGpuTest::stream_status() const {
    return std::atomic_ref<Status>(control_->status).load(std::memory_order_acquire);
}

void HostGpuTest::destroy_device() {
    writer_->flush();
    device_->destroy();
    is_stream_open_ = false;
    for (const auto& region : regions_) {
        region->is_released = true;
    }
}

void HostGpuTest::close_stream() {
    if (!is_stream_open_) {
        return;
    }
    writer_->flush();
    device_->close_stream(stream_.stream);
    is_stream_open_ = false;
}

} // namespace directmetal::host::gpu_testing
