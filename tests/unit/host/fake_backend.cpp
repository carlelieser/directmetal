#include "fake_backend.hpp"

#include <algorithm>

namespace directmetal::host::testing {
namespace {

using backend::Result;

class FakeBuffer final : public backend::Buffer {
public:
    FakeBuffer(FakeGpu& gpu, std::uint64_t size, bool is_counted)
        : gpu_(gpu), size_(size), is_counted_(is_counted) {
        ++gpu_.live_buffers;
        gpu_.used_bytes += is_counted_ ? size_ : 0;
    }
    ~FakeBuffer() override {
        --gpu_.live_buffers;
        gpu_.used_bytes -= is_counted_ ? size_ : 0;
    }

    std::uint64_t size() const noexcept override { return size_; }
    std::byte* cpu_address() noexcept override { return nullptr; }
    void set_label(std::string_view label) override { gpu_.record("label " + std::string(label)); }

private:
    FakeGpu& gpu_;
    std::uint64_t size_;
    bool is_counted_;
};

class FakeTexture final : public backend::Texture {
public:
    FakeTexture(FakeGpu& gpu, const backend::TextureDesc& desc) : gpu_(gpu), desc_(desc) {
        ++gpu_.live_textures;
        gpu_.used_bytes += texture_bytes;
    }
    ~FakeTexture() override {
        --gpu_.live_textures;
        gpu_.used_bytes -= texture_bytes;
    }

    const backend::TextureDesc& desc() const noexcept override { return desc_; }
    void set_label(std::string_view label) override { gpu_.record("label " + std::string(label)); }

    static constexpr std::uint64_t texture_bytes = 4096;

private:
    FakeGpu& gpu_;
    backend::TextureDesc desc_;
};

struct PendingSignal {
    FakeTimeline* timeline;
    std::weak_ptr<int> token;
    std::uint64_t value;
};

} // namespace

class FakeQueue final : public backend::Queue, public backend::CommandRecorder {
public:
    explicit FakeQueue(FakeGpu& gpu) : gpu_(gpu), progress_(0) { gpu_.add_queue(this); }
    ~FakeQueue() override { gpu_.remove_queue(this); }

    backend::CommandRecorder& recorder() noexcept override { return *this; }
    backend::Status submit() noexcept override {
        {
            const std::lock_guard lock(mutex_);
            ++submitted_;
            submitted_signals_.insert(submitted_signals_.end(), recorded_signals_.begin(),
                                      recorded_signals_.end());
            recorded_signals_.clear();
        }
        if (gpu_.is_auto_complete) {
            complete();
        }
        return backend::Status::success;
    }
    std::uint64_t last_submitted_value() const noexcept override {
        const std::lock_guard lock(mutex_);
        return submitted_;
    }
    backend::Timeline& progress() noexcept override { return progress_; }

    void complete() {
        std::vector<PendingSignal> signals;
        std::uint64_t submitted = 0;
        {
            const std::lock_guard lock(mutex_);
            signals.swap(submitted_signals_);
            submitted = submitted_;
        }
        for (const PendingSignal& signal : signals) {
            if (const auto alive = signal.token.lock()) {
                signal.timeline->signal_cpu(signal.value);
            }
        }
        progress_.signal_cpu(submitted);
    }

    void clear_texture(backend::Texture&, backend::SubresourceRange, backend::ClearColor) override {
        gpu_.record("clear_texture");
    }
    void clear_depth_stencil(backend::Texture&, backend::SubresourceRange,
                             const backend::DepthStencilClear&) override {
        gpu_.record("clear_depth_stencil");
    }
    void copy_buffer(const backend::BufferCopy&) override { gpu_.record("copy_buffer"); }
    void copy_buffer_to_texture(const backend::BufferTextureCopy&) override {
        gpu_.record("copy_buffer_to_texture");
    }
    void copy_texture_to_buffer(const backend::BufferTextureCopy&) override {
        gpu_.record("copy_texture_to_buffer");
    }
    void copy_texture(const backend::TextureCopy&) override { gpu_.record("copy_texture"); }
    void signal(backend::Timeline& timeline, std::uint64_t value) override {
        gpu_.record("signal " + std::to_string(value));
        auto& fake = dynamic_cast<FakeTimeline&>(timeline);
        const std::lock_guard lock(mutex_);
        recorded_signals_.push_back({&fake, fake.token(), value});
    }

private:
    FakeGpu& gpu_;
    FakeTimeline progress_;
    mutable std::mutex mutex_;
    std::uint64_t submitted_ = 0;
    std::vector<PendingSignal> recorded_signals_;
    std::vector<PendingSignal> submitted_signals_;
};

FakeTimeline::~FakeTimeline() {
    for (auto& waiter : waiting_) {
        waiter.second();
    }
}

std::uint64_t FakeTimeline::completed_value() const noexcept {
    const std::lock_guard lock(mutex_);
    return completed_;
}

backend::Status FakeTimeline::wait(std::uint64_t value, std::chrono::nanoseconds timeout) noexcept {
    std::unique_lock lock(mutex_);
    const auto is_reached = [this, value] { return completed_ >= value; };
    if (timeout == std::chrono::nanoseconds::max()) {
        has_advanced_.wait(lock, is_reached);
        return backend::Status::success;
    }
    return has_advanced_.wait_for(lock, timeout, is_reached) ? backend::Status::success
                                                             : backend::Status::timeout;
}

void FakeTimeline::signal_cpu(std::uint64_t value) noexcept {
    std::vector<std::function<void()>> ready;
    {
        const std::lock_guard lock(mutex_);
        completed_ = std::max(completed_, value);
        std::erase_if(waiting_, [&](auto& waiter) {
            if (waiter.first > completed_) {
                return false;
            }
            ready.push_back(std::move(waiter.second));
            return true;
        });
    }
    has_advanced_.notify_all();
    for (auto& callback : ready) {
        callback();
    }
}

void FakeTimeline::notify(std::uint64_t value, std::function<void()> callback) {
    {
        const std::lock_guard lock(mutex_);
        if (completed_ < value) {
            waiting_.emplace_back(value, std::move(callback));
            return;
        }
    }
    callback();
}

std::vector<std::string> FakeGpu::recorded() const {
    const std::lock_guard lock(mutex_);
    return recorded_;
}

void FakeGpu::record(std::string name) {
    const std::lock_guard lock(mutex_);
    recorded_.push_back(std::move(name));
}

void FakeGpu::complete_all() {
    std::vector<FakeQueue*> queues;
    {
        const std::lock_guard lock(mutex_);
        queues = queues_;
    }
    for (FakeQueue* queue : queues) {
        queue->complete();
    }
}

std::size_t FakeGpu::queue_count() const {
    const std::lock_guard lock(mutex_);
    return queues_.size();
}

std::uint64_t FakeGpu::submitted(std::size_t queue) const {
    const std::lock_guard lock(mutex_);
    return queues_.at(queue)->last_submitted_value();
}

void FakeGpu::add_queue(FakeQueue* queue) {
    const std::lock_guard lock(mutex_);
    queues_.push_back(queue);
    ++live_queues;
}

void FakeGpu::remove_queue(FakeQueue* queue) {
    const std::lock_guard lock(mutex_);
    std::erase(queues_, queue);
    --live_queues;
}

Result<std::unique_ptr<backend::Buffer>>
FakeDevice::create_buffer(const backend::BufferDesc& desc) {
    if (desc.storage == backend::StorageMode::memoryless) {
        return Result<std::unique_ptr<backend::Buffer>>::failure(backend::Status::unsupported);
    }
    return Result<std::unique_ptr<backend::Buffer>>::success(
        std::make_unique<FakeBuffer>(gpu_, desc.size, true));
}

Result<std::unique_ptr<backend::Buffer>> FakeDevice::import_memory(std::span<std::byte> memory) {
    return Result<std::unique_ptr<backend::Buffer>>::success(
        std::make_unique<FakeBuffer>(gpu_, memory.size(), false));
}

Result<std::unique_ptr<backend::Texture>>
FakeDevice::create_texture(const backend::TextureDesc& desc) {
    return Result<std::unique_ptr<backend::Texture>>::success(
        std::make_unique<FakeTexture>(gpu_, desc));
}

Result<std::unique_ptr<backend::Timeline>>
FakeDevice::create_timeline(std::uint64_t initial_value) {
    return Result<std::unique_ptr<backend::Timeline>>::success(
        std::make_unique<FakeTimeline>(initial_value));
}

Result<std::unique_ptr<backend::Queue>> FakeDevice::create_queue() {
    return Result<std::unique_ptr<backend::Queue>>::success(std::make_unique<FakeQueue>(gpu_));
}

backend::MemoryStats FakeDevice::memory_stats() const {
    return {0, 0, gpu_.used_bytes.load(), 0, 0};
}

Result<std::unique_ptr<backend::Device>>
FakeProvider::create(const backend::DeviceRequest& request) {
    last_request = request;
    if (create_status != backend::Status::success) {
        return Result<std::unique_ptr<backend::Device>>::failure(create_status);
    }
    return Result<std::unique_ptr<backend::Device>>::success(
        std::make_unique<FakeDevice>(gpu_, capabilities()));
}

std::vector<backend::Capabilities> FakeProvider::adapters() {
    return {capabilities()};
}

backend::Capabilities FakeProvider::capabilities() {
    backend::Capabilities capabilities{};
    capabilities.family = backend::GpuFamily::apple9;
    capabilities.max_texture_2d = 16384;
    capabilities.max_texture_3d = 2048;
    capabilities.max_buffer_length = std::uint64_t{1} << 30;
    capabilities.registry_id = 42;
    capabilities.name = {'F', 'a', 'k', 'e'};
    return capabilities;
}

} // namespace directmetal::host::testing
