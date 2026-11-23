#pragma once

#include <directmetal/bridge/commands.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace directmetal::bridge {

// Encodes commands into pooled shared-memory chunks and submits them to one stream. Used by one
// thread at a time. Flushes only when a chunk is full, on flush(), and before the caller waits on
// the CPU; errors reported by the unix side are sticky and surface through status(). Recording a
// submit command flushes. Chunks are pooled and reused once the unix side has retired them; the
// writer is destroyed only after close_stream or destroy_device has returned for its stream.
class StreamWriter {
public:
    static constexpr std::uint32_t default_chunk_size = 64 * 1024;

    StreamWriter(Handle stream, StreamControl& control,
                 std::uint32_t chunk_size = default_chunk_size);
    StreamWriter(const StreamWriter&) = delete;
    StreamWriter& operator=(const StreamWriter&) = delete;
    ~StreamWriter();

    template <typename Command> void record(const Command& command) {
        static_assert(opcode_of<Command> != Opcode::count, "not a command payload type");
        static_assert(!layout_of(opcode_of<Command>).has_inline_data, "use record_label");
        record_payload(opcode_of<Command>, &command, sizeof(Command));
    }

    // label is cut before its first byte that is not well-formed UTF-8, and to at most
    // max_label_length bytes at a code point boundary.
    void record_label(Handle object, std::string_view label);
    Status flush() noexcept;
    // Sequence of the next chunk to be submitted.
    std::uint64_t sequence() const noexcept;
    Status status() const noexcept;

private:
    struct Chunk {
        std::byte* memory;
        // The sequence it was submitted with: reusable once retired_sequence reaches it.
        std::uint64_t sequence;
    };

    void record_payload(Opcode opcode, const void* payload, std::uint32_t size);
    // Space for the body of a command of size bytes after its header, or null after an error.
    std::byte* begin_command(Opcode opcode, std::uint32_t size);
    bool open_chunk();

    Handle stream_;
    StreamControl* control_;
    std::uint32_t chunk_size_;
    std::vector<Chunk> pool_;
    // Index into pool_ of the chunk being recorded, if any.
    std::size_t current_;
    std::uint32_t offset_ = 0;
    std::uint32_t command_count_ = 0;
    std::uint64_t sequence_ = 1;
    // A failure on this side: an invalid chunk size, exhausted shared memory or a failed submit.
    Status error_ = status_success;
};

} // namespace directmetal::bridge
