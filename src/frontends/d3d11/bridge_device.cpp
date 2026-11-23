#include "bridge_device.hpp"

#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/shared_block_allocator.hpp>

#include <string_view>

namespace directmetal::d3d11 {
namespace {

// 32-bit PE code reaches the unix side only through Wine's WoW64 thunks.
constexpr bool is_wow64_process = sizeof(void*) == 4;

formats::DeviceFeatures features_of(const backend::Capabilities& capabilities) noexcept {
    formats::DeviceFeatures features = formats::mac_features(capabilities.family);
    features.has_bc_compression = capabilities.has_bc != 0;
    return features;
}

} // namespace

backend::Result<std::unique_ptr<BridgeDevice>> BridgeDevice::open() {
    using OpenResult = backend::Result<std::unique_ptr<BridgeDevice>>;
    if (const backend::Status status = bridge::initialize(std::string_view{}, is_wow64_process);
        status != backend::Status::success) {
        return OpenResult::failure(status);
    }
    auto client = bridge::DeviceClient::create(0, bridge::DeviceOptions{});
    if (!client.has_value()) {
        return OpenResult::failure(client.status());
    }
    std::unique_ptr<BridgeDevice> device(new BridgeDevice(std::move(client).value()));
    if (const backend::Status status = device->open_stream(); status != backend::Status::success) {
        return OpenResult::failure(status);
    }
    return OpenResult::success(std::move(device));
}

BridgeDevice::BridgeDevice(bridge::DeviceClient client) noexcept
    : client_(std::move(client)), features_(features_of(client_.capabilities())) {}

BridgeDevice::~BridgeDevice() {
    if (writer_.has_value()) {
        writer_->flush();
    }
    client_.destroy();
    // The chunks and the control block are freed only after destroy_device has returned.
    writer_.reset();
    if (control_ != nullptr) {
        bridge::shared_blocks().free(control_, sizeof(bridge::StreamControl));
    }
}

backend::Status BridgeDevice::open_stream() {
    control_ = bridge::shared_blocks().allocate_stream_control();
    if (control_ == nullptr) {
        return backend::Status::out_of_memory;
    }
    auto opened = client_.open_stream(*control_);
    if (!opened.has_value()) {
        return opened.status();
    }
    writer_.emplace(opened.value().stream, *control_);
    progress_.emplace(opened.value().progress_timeline, &control_->completed_progress);
    return backend::Status::success;
}

const formats::DeviceFeatures& BridgeDevice::features() const noexcept {
    return features_;
}

bool BridgeDevice::is_lost() noexcept {
    const std::lock_guard lock(mutex_);
    return writer_->status() < 0;
}

backend::Result<bridge::Handle> BridgeDevice::create_texture(const backend::TextureDesc& desc) {
    return client_.create_texture(desc);
}

backend::Result<bridge::Handle> BridgeDevice::import_memory(std::span<std::byte> memory) {
    auto imported = client_.import_memory(memory);
    if (!imported.has_value()) {
        return backend::Result<bridge::Handle>::failure(imported.status());
    }
    return backend::Result<bridge::Handle>::success(imported.value().buffer);
}

// The lock is held across release_memory, which drains the stream and waits for the GPU: the
// teardown contract requires the stream to be flushed with nothing unsubmitted when the call is
// made, and another thread recording in between would break that. Every other stream user on this
// device blocks for that drain, so releasing a staging texture costs a full GPU wait.
backend::Status BridgeDevice::release_memory(bridge::Handle buffer) noexcept {
    const std::lock_guard lock(mutex_);
    if (has_unsubmitted_) {
        submit_locked();
    }
    return client_.release_memory(buffer);
}

backend::Status BridgeDevice::wait(std::uint64_t value, std::chrono::nanoseconds timeout) {
    {
        const std::lock_guard lock(mutex_);
        if (value > submitted_) {
            submit_locked();
        }
        if (writer_->status() < 0) {
            return backend::Status::device_lost;
        }
    }
    return progress_->wait(value, timeout);
}

void BridgeDevice::submit_locked() {
    writer_->record(bridge::SubmitCommand{0});
    ++submitted_;
    has_unsubmitted_ = false;
}

} // namespace directmetal::d3d11
