#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/staging_ring.hpp>

#include <utility>

namespace directmetal::bridge {
namespace {

using RingResult = backend::Result<StagingRing>;

// capacity rounded up to shared_memory_alignment; 0 when it is 0 or rounding overflows.
constexpr std::uint64_t import_size(std::uint64_t capacity) noexcept {
    constexpr std::uint64_t mask = shared_memory_alignment - 1;
    const bool is_representable = capacity != 0 && capacity <= UINT64_MAX - mask;
    return is_representable ? (capacity + mask) & ~mask : 0;
}

} // namespace

backend::Result<StagingRing> StagingRing::create(DeviceClient& device, std::uint64_t capacity) {
    const std::uint64_t size = import_size(capacity);
    if (size == 0) {
        return RingResult::failure(backend::Status::invalid_argument);
    }
    auto* memory = static_cast<std::byte*>(directmetal_bridge_allocate_shared(size));
    if (memory == nullptr) {
        return RingResult::failure(backend::Status::out_of_memory);
    }
    const std::span<std::byte> span{memory, static_cast<std::size_t>(size)};
    auto imported = device.import_memory(span);
    if (!imported.has_value()) {
        directmetal_bridge_free_shared(memory, size);
        return RingResult::failure(imported.status());
    }
    // Owning the import from here on releases it on every path below.
    StagingRing ring(device, span, imported.value().buffer);
    if (!imported.value().is_zero_copy) {
        return RingResult::failure(backend::Status::unsupported);
    }
    return RingResult::success(std::move(ring));
}

StagingRing::StagingRing(DeviceClient& device, std::span<std::byte> memory, Handle buffer)
    : device_(&device), memory_(memory), buffer_(buffer), ring_(memory.size()) {}

StagingRing::StagingRing(StagingRing&& other) noexcept
    : device_(other.device_), memory_(std::exchange(other.memory_, {})),
      buffer_(std::exchange(other.buffer_, {})), ring_(std::move(other.ring_)) {}

StagingRing::~StagingRing() {
    if (memory_.empty()) {
        return;
    }
    // A failed release is logged by the client; the memory may still be in use, so it leaks.
    if (device_->release_memory(buffer_) == backend::Status::success) {
        directmetal_bridge_free_shared(memory_.data(), memory_.size());
    }
}

std::optional<StagingRing::Allocation>
StagingRing::allocate(std::uint64_t size, std::uint64_t alignment, std::uint64_t retire_value) {
    const std::optional<std::uint64_t> offset = ring_.allocate(size, alignment, retire_value);
    if (!offset.has_value()) {
        return std::nullopt;
    }
    return Allocation{memory_.data() + static_cast<std::size_t>(*offset), buffer_, *offset};
}

void StagingRing::retire(std::uint64_t completed_value) {
    ring_.retire(completed_value);
}

std::uint64_t StagingRing::capacity() const noexcept {
    return memory_.size();
}

} // namespace directmetal::bridge
