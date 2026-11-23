#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/protocol.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

// The one-way command stream. A chunk is a ChunkHeader followed by command_count commands; each
// command is a CommandHeader followed by its payload struct and, for set_label, inline UTF-8.
// Commands are 8-byte aligned and their size is a multiple of 8. Payload sizes must match
// exactly: directmetal.dll and directmetal.so always ship together.
namespace directmetal::bridge {

inline constexpr std::uint32_t chunk_magic = 0x53434D44; // "DMCS" in memory order
inline constexpr std::uint16_t stream_version = 1;
inline constexpr std::uint32_t max_label_length = 256;
// Chunks are allocated with directmetal_bridge_allocate_shared, so they are 16 KiB aligned.
inline constexpr std::uint32_t max_chunk_size = 1024 * 1024;

struct alignas(8) ChunkHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t reserved;
    std::uint64_t sequence;
    // Bytes including this header.
    std::uint32_t size;
    std::uint32_t command_count;
};

enum class Opcode : std::uint16_t {
    nop,
    set_label,
    destroy_object,
    clear_texture,
    clear_depth_stencil,
    copy_buffer,
    copy_buffer_to_texture,
    copy_texture_to_buffer,
    copy_texture,
    signal_timeline,
    submit,
    count,
};

struct alignas(8) CommandHeader {
    Opcode opcode;
    std::uint16_t reserved;
    // Bytes including this header and any inline data padding.
    std::uint32_t size;
};

// Followed by length bytes of UTF-8, padded with zeros to a multiple of 8.
struct alignas(8) SetLabelCommand {
    Handle object;
    std::uint32_t length;
    std::uint32_t reserved;
};

// Buffers and textures created with create_buffer or create_texture. Imported buffers are released
// with release_memory and timelines with destroy_timeline; naming either here is a stream error.
struct alignas(8) DestroyObjectCommand {
    Handle object;
};

struct alignas(8) ClearTextureCommand {
    Handle texture;
    backend::SubresourceRange range;
    backend::ClearColor color;
    std::uint32_t reserved;
};

struct alignas(8) ClearDepthStencilCommand {
    Handle texture;
    backend::SubresourceRange range;
    backend::DepthStencilClear clear;
};

struct alignas(8) CopyBufferCommand {
    Handle source;
    std::uint64_t source_offset;
    Handle destination;
    std::uint64_t destination_offset;
    std::uint64_t size;
};

// Payload of both copy_buffer_to_texture and copy_texture_to_buffer.
struct alignas(8) BufferTextureCopyCommand {
    Handle buffer;
    std::uint64_t offset;
    std::uint32_t row_pitch;
    std::uint32_t slice_pitch;
    Handle texture;
    std::uint32_t mip;
    std::uint32_t slice;
    backend::Origin3D origin;
    backend::Extent3D extent;
};

struct alignas(8) CopyTextureCommand {
    Handle source;
    std::uint32_t source_mip;
    std::uint32_t source_slice;
    backend::Origin3D source_origin;
    std::uint32_t reserved0;
    Handle destination;
    std::uint32_t destination_mip;
    std::uint32_t destination_slice;
    backend::Origin3D destination_origin;
    backend::Extent3D extent;
    std::uint32_t reserved1[2];
};

struct alignas(8) SignalTimelineCommand {
    Handle timeline;
    std::uint64_t value;
};

// Commits the commands decoded so far and signals the stream's progress timeline.
struct alignas(8) SubmitCommand {
    std::uint64_t reserved;
};

// PE-allocated, 64-byte aligned, written only by the unix side. A chunk's memory may be reused once
// retired_sequence reaches its sequence (decoded, not GPU-complete). completed_progress mirrors the
// stream's progress timeline. A decode failure or stale handle makes status sticky; later chunks
// are ignored and waits on the stream's timelines fail with status_device_removed.
struct alignas(64) StreamControl {
    std::uint64_t retired_sequence;
    Status status;
    std::uint32_t failed_opcode;
    std::uint64_t failed_sequence;
    std::uint64_t completed_progress;
    std::uint64_t reserved[4];
};

template <typename Command> inline constexpr Opcode opcode_of = Opcode::count;
template <> inline constexpr Opcode opcode_of<SetLabelCommand> = Opcode::set_label;
template <> inline constexpr Opcode opcode_of<DestroyObjectCommand> = Opcode::destroy_object;
template <> inline constexpr Opcode opcode_of<ClearTextureCommand> = Opcode::clear_texture;
template <>
inline constexpr Opcode opcode_of<ClearDepthStencilCommand> = Opcode::clear_depth_stencil;
template <> inline constexpr Opcode opcode_of<CopyBufferCommand> = Opcode::copy_buffer;
template <> inline constexpr Opcode opcode_of<CopyTextureCommand> = Opcode::copy_texture;
template <> inline constexpr Opcode opcode_of<SignalTimelineCommand> = Opcode::signal_timeline;
template <> inline constexpr Opcode opcode_of<SubmitCommand> = Opcode::submit;

static_assert(sizeof(ChunkHeader) == 24);
static_assert(offsetof(ChunkHeader, sequence) == 8);
static_assert(offsetof(ChunkHeader, size) == 16);
static_assert(offsetof(ChunkHeader, command_count) == 20);
static_assert(sizeof(CommandHeader) == 8 && offsetof(CommandHeader, size) == 4);
static_assert(sizeof(SetLabelCommand) == 16 && offsetof(SetLabelCommand, length) == 8);
static_assert(sizeof(DestroyObjectCommand) == 8);
static_assert(sizeof(ClearTextureCommand) == 48);
static_assert(offsetof(ClearTextureCommand, range) == 8);
static_assert(offsetof(ClearTextureCommand, color) == 24);
static_assert(sizeof(ClearDepthStencilCommand) == 32);
static_assert(offsetof(ClearDepthStencilCommand, clear) == 24);
static_assert(sizeof(CopyBufferCommand) == 40);
static_assert(offsetof(CopyBufferCommand, destination) == 16);
static_assert(offsetof(CopyBufferCommand, size) == 32);
static_assert(sizeof(BufferTextureCopyCommand) == 64);
static_assert(offsetof(BufferTextureCopyCommand, row_pitch) == 16);
static_assert(offsetof(BufferTextureCopyCommand, texture) == 24);
static_assert(offsetof(BufferTextureCopyCommand, origin) == 40);
static_assert(offsetof(BufferTextureCopyCommand, extent) == 52);
static_assert(sizeof(CopyTextureCommand) == 80);
static_assert(offsetof(CopyTextureCommand, source_origin) == 16);
static_assert(offsetof(CopyTextureCommand, destination) == 32);
static_assert(offsetof(CopyTextureCommand, destination_origin) == 48);
static_assert(offsetof(CopyTextureCommand, extent) == 60);
static_assert(sizeof(SignalTimelineCommand) == 16);
static_assert(sizeof(SubmitCommand) == 8);
static_assert(sizeof(StreamControl) == 64);
static_assert(offsetof(StreamControl, status) == 8);
static_assert(offsetof(StreamControl, failed_opcode) == 12);
static_assert(offsetof(StreamControl, failed_sequence) == 16);
static_assert(offsetof(StreamControl, completed_progress) == 24);
static_assert(offsetof(StreamControl, reserved) == 32);
// The PE side reads StreamControl through std::atomic_ref, including from 32-bit processes.
static_assert(std::atomic_ref<std::uint64_t>::is_always_lock_free);

} // namespace directmetal::bridge
