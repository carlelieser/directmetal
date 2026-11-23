#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/command_codec.hpp>
#include <directmetal/bridge/control.hpp>
#include <directmetal/bridge/stream_writer.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace directmetal::bridge {
namespace {

constexpr std::size_t no_chunk = SIZE_MAX;
// Chunk::sequence of the chunk being recorded; never retired.
constexpr std::uint64_t recording = UINT64_MAX;

bool is_continuation_byte(char value) noexcept {
    return (static_cast<unsigned char>(value) & 0xC0) == 0x80;
}

std::uint32_t truncated_length(std::string_view label) noexcept {
    if (label.size() <= max_label_length) {
        return static_cast<std::uint32_t>(label.size());
    }
    std::uint32_t length = max_label_length;
    while (length > 0 && is_continuation_byte(label[length])) {
        --length;
    }
    return length;
}

} // namespace

StreamWriter::StreamWriter(Handle stream, StreamControl& control, std::uint32_t chunk_size)
    : stream_(stream), control_(&control), chunk_size_(chunk_size), current_(no_chunk) {
    const bool is_in_range = chunk_size >= min_chunk_size && chunk_size <= max_chunk_size;
    if (!is_in_range || chunk_size % command_alignment != 0) {
        error_ = status_invalid_parameter;
    }
}

StreamWriter::~StreamWriter() {
    for (const Chunk& chunk : pool_) {
        directmetal_bridge_free_shared(chunk.memory, chunk_size_);
    }
}

void StreamWriter::record_label(Handle object, std::string_view label) {
    const std::uint32_t length = truncated_length(label);
    std::byte* body = begin_command(Opcode::set_label, command_size(Opcode::set_label, length));
    if (body == nullptr) {
        return;
    }
    const SetLabelCommand command{object, length, 0};
    std::memcpy(body, &command, sizeof(command));
    std::memcpy(body + sizeof(command), label.data(), length);
}

Status StreamWriter::flush() noexcept {
    if (current_ == no_chunk) {
        return status();
    }
    Chunk& chunk = pool_[current_];
    current_ = no_chunk;
    if (status() < 0 || command_count_ == 0) {
        chunk.sequence = 0;
        return status();
    }
    const ChunkHeader header{chunk_magic, stream_version, 0, sequence_, offset_, command_count_};
    std::memcpy(chunk.memory, &header, sizeof(header));
    chunk.sequence = sequence_;
    SubmitStreamParams params{stream_, bridge::to_address(chunk.memory), offset_, 0, sequence_};
    ++sequence_;
    if (const Status result = call(FunctionCode::submit_stream, &params); result < 0) {
        error_ = result;
    }
    return status();
}

std::uint64_t StreamWriter::sequence() const noexcept {
    return sequence_;
}

Status StreamWriter::status() const noexcept {
    if (error_ < 0) {
        return error_;
    }
    return std::atomic_ref<Status>(control_->status).load(std::memory_order_acquire);
}

void StreamWriter::record_payload(Opcode opcode, const void* payload, std::uint32_t size) {
    std::byte* body = begin_command(opcode, command_size(opcode, 0));
    if (body != nullptr) {
        std::memcpy(body, payload, size);
    }
    if (opcode == Opcode::submit) {
        flush();
    }
}

std::byte* StreamWriter::begin_command(Opcode opcode, std::uint32_t size) {
    if (current_ != no_chunk && offset_ + size > chunk_size_) {
        flush();
    }
    if (error_ < 0) {
        return nullptr;
    }
    if (current_ == no_chunk && !open_chunk()) {
        return nullptr;
    }
    // Pooled chunks hold earlier commands, and padding must be zero.
    std::byte* command = pool_[current_].memory + offset_;
    std::memset(command, 0, size);
    const CommandHeader header{opcode, 0, size};
    std::memcpy(command, &header, sizeof(header));
    offset_ += size;
    ++command_count_;
    return command + sizeof(header);
}

bool StreamWriter::open_chunk() {
    const std::uint64_t retired =
        std::atomic_ref<std::uint64_t>(control_->retired_sequence).load(std::memory_order_acquire);
    auto reusable = std::find_if(pool_.begin(), pool_.end(), [retired](const Chunk& chunk) {
        return chunk.sequence <= retired;
    });
    if (reusable == pool_.end()) {
        // Reserved first, so a throwing push_back cannot leak the shared memory.
        pool_.reserve(pool_.size() + 1);
        void* memory = directmetal_bridge_allocate_shared(chunk_size_);
        if (memory == nullptr) {
            error_ = status_no_memory;
            return false;
        }
        pool_.push_back({static_cast<std::byte*>(memory), 0});
        reusable = pool_.end() - 1;
    }
    reusable->sequence = recording;
    current_ = static_cast<std::size_t>(reusable - pool_.begin());
    offset_ = sizeof(ChunkHeader);
    command_count_ = 0;
    return true;
}

} // namespace directmetal::bridge
