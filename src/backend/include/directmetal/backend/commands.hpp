#pragma once

#include <directmetal/backend/types.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string_view>

namespace directmetal::backend {

class Buffer {
public:
    virtual ~Buffer() = default;
    virtual std::uint64_t size() const noexcept = 0;
    // Null unless the buffer's storage is CPU-visible.
    virtual std::byte* cpu_address() noexcept = 0;
    virtual void set_label(std::string_view label) = 0;
};

class Texture {
public:
    virtual ~Texture() = default;
    virtual const TextureDesc& desc() const noexcept = 0;
    virtual void set_label(std::string_view label) = 0;
};

// A monotonically increasing 64-bit counter backed by MTLSharedEvent. Thread-safe.
class Timeline {
public:
    virtual ~Timeline() = default;
    virtual std::uint64_t completed_value() const noexcept = 0;
    virtual Status wait(std::uint64_t value, std::chrono::nanoseconds timeout) noexcept = 0;
    virtual void signal_cpu(std::uint64_t value) noexcept = 0;
    // Runs callback once completed_value() reaches value, immediately if it already has. The
    // callback runs on a Metal completion thread and must only touch atomics and lock-free queues.
    // Every pending callback still runs when the timeline is destroyed or its device is lost, so
    // deferred frees waiting on it are not stranded.
    virtual void notify(std::uint64_t value, std::function<void()> callback) = 0;
};

struct BufferCopy {
    Buffer& source;
    std::uint64_t source_offset;
    Buffer& destination;
    std::uint64_t destination_offset;
    std::uint64_t size;
};

struct BufferTextureCopy {
    Buffer& buffer;
    std::uint64_t offset;
    std::uint32_t row_pitch;
    std::uint32_t slice_pitch;
    Texture& texture;
    std::uint32_t mip;
    std::uint32_t slice;
    Origin3D origin;
    Extent3D extent;
};

struct TextureCopy {
    Texture& source;
    std::uint32_t source_mip;
    std::uint32_t source_slice;
    Origin3D source_origin;
    Texture& destination;
    std::uint32_t destination_mip;
    std::uint32_t destination_slice;
    Origin3D destination_origin;
    Extent3D extent;
};

// Records into the queue's current command buffer. Used by one thread at a time; valid until
// the next Queue::submit.
class CommandRecorder {
public:
    virtual ~CommandRecorder() = default;
    // Render-pass load actions; no shaders are involved.
    virtual void clear_texture(Texture& texture, SubresourceRange range, ClearColor color) = 0;
    virtual void clear_depth_stencil(Texture& texture, SubresourceRange range,
                                     const DepthStencilClear& clear) = 0;
    virtual void copy_buffer(const BufferCopy& copy) = 0;
    virtual void copy_buffer_to_texture(const BufferTextureCopy& copy) = 0;
    virtual void copy_texture_to_buffer(const BufferTextureCopy& copy) = 0;
    virtual void copy_texture(const TextureCopy& copy) = 0;
    virtual void signal(Timeline& timeline, std::uint64_t value) = 0;
};

// Owned by one thread at a time (the stream thread that decodes into it). progress() starts at 0
// and the k-th submit signals k.
class Queue {
public:
    virtual ~Queue() = default;
    virtual CommandRecorder& recorder() noexcept = 0;
    // Commits the current MTLCommandBuffer and signals progress() with last_submitted_value() + 1.
    virtual Status submit() noexcept = 0;
    // The value the most recent submit signals; 0 before the first submit. Thread-safe.
    virtual std::uint64_t last_submitted_value() const noexcept = 0;
    // Stable for the queue's lifetime. Thread-safe.
    virtual Timeline& progress() noexcept = 0;
};

} // namespace directmetal::backend
