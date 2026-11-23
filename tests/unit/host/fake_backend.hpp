#pragma once

#include <directmetal/backend/device.hpp>
#include <directmetal/host/dispatch.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// A backend without a GPU: queues record what they are asked to do and complete submitted work
// on request (or at once while FakeGpu::is_auto_complete is set).
namespace directmetal::host::testing {

class FakeTimeline final : public backend::Timeline {
public:
    explicit FakeTimeline(std::uint64_t initial_value) : completed_(initial_value) {}
    ~FakeTimeline() override;

    std::uint64_t completed_value() const noexcept override;
    backend::Status wait(std::uint64_t value, std::chrono::nanoseconds timeout) noexcept override;
    void signal_cpu(std::uint64_t value) noexcept override;
    void notify(std::uint64_t value, std::function<void()> callback) override;

    // Expires when the timeline is destroyed.
    std::weak_ptr<int> token() const noexcept { return token_; }

private:
    std::shared_ptr<int> token_ = std::make_shared<int>(0);
    mutable std::mutex mutex_;
    std::condition_variable has_advanced_;
    std::uint64_t completed_;
    std::vector<std::pair<std::uint64_t, std::function<void()>>> waiting_;
};

class FakeQueue;

// What the fake backend has done, shared by every fake object of one test.
class FakeGpu {
public:
    // Recorder calls, e.g. "clear_texture", in order across every queue.
    std::vector<std::string> recorded() const;
    void record(std::string name);
    // Completes everything submitted on every live queue.
    void complete_all();
    std::size_t queue_count() const;
    std::uint64_t submitted(std::size_t queue) const;

    // While held, create_buffer and every recorder call block after announcing that they were
    // entered.
    void hold_gate();
    void wait_gate_entered();
    void release_gate();
    void pass_gate();

    void add_queue(FakeQueue* queue);
    void remove_queue(FakeQueue* queue);

    std::atomic<bool> is_auto_complete = true;
    std::atomic<int> live_buffers = 0;
    std::atomic<int> live_textures = 0;
    std::atomic<int> live_queues = 0;
    std::atomic<std::uint64_t> used_bytes = 0;

private:
    mutable std::mutex mutex_;
    std::vector<std::string> recorded_;
    std::vector<FakeQueue*> queues_;
    std::condition_variable gate_changed_;
    bool is_gate_held_ = false;
    bool is_gate_entered_ = false;
};

class FakeDevice final : public backend::Device {
public:
    FakeDevice(FakeGpu& gpu, const backend::Capabilities& capabilities)
        : gpu_(gpu), capabilities_(capabilities) {}

    const backend::Capabilities& capabilities() const noexcept override { return capabilities_; }
    backend::Result<std::unique_ptr<backend::Buffer>>
    create_buffer(const backend::BufferDesc& desc) override;
    backend::Result<std::unique_ptr<backend::Buffer>>
    import_memory(std::span<std::byte> memory) override;
    backend::Result<std::unique_ptr<backend::Texture>>
    create_texture(const backend::TextureDesc& desc) override;
    backend::Result<std::unique_ptr<backend::Timeline>>
    create_timeline(std::uint64_t initial_value) override;
    backend::Result<std::unique_ptr<backend::Queue>> create_queue() override;
    backend::MemoryStats memory_stats() const override;

private:
    FakeGpu& gpu_;
    backend::Capabilities capabilities_;
};

class FakeProvider final : public DeviceProvider {
public:
    explicit FakeProvider(FakeGpu& gpu) : gpu_(gpu) {}

    backend::Result<std::unique_ptr<backend::Device>>
    create(const backend::DeviceRequest& request) override;
    std::vector<backend::Capabilities> adapters() override;

    static backend::Capabilities capabilities();

    backend::DeviceRequest last_request{};
    // What created devices report.
    backend::Capabilities device_capabilities = capabilities();
    // create fails with this status unless it is success.
    backend::Status create_status = backend::Status::success;

private:
    FakeGpu& gpu_;
};

} // namespace directmetal::host::testing
