#pragma once

#include <directmetal/bridge/commands.hpp>

#include <cstdint>
#include <string_view>

namespace directmetal::bridge {

// Encodes commands into pooled shared-memory chunks and submits them to one stream. Used by one
// thread at a time. Flushes only when a chunk is full, on flush(), and before the caller waits on
// the CPU; errors reported by the unix side are sticky and surface through status().
class StreamWriter {
public:
    static constexpr std::uint32_t default_chunk_size = 64 * 1024;

    StreamWriter(Handle stream, StreamControl& control,
                 std::uint32_t chunk_size = default_chunk_size);
    StreamWriter(const StreamWriter&) = delete;
    StreamWriter& operator=(const StreamWriter&) = delete;
    ~StreamWriter();

    template <typename Command> void record(const Command& command) {
        static_assert(opcode_of<Command> != Opcode::count, "command needs an explicit opcode");
        record_payload(opcode_of<Command>, &command, sizeof(Command));
    }

    // For payloads shared by several opcodes, such as BufferTextureCopyCommand.
    template <typename Command> void record(Opcode opcode, const Command& command) {
        record_payload(opcode, &command, sizeof(Command));
    }

    // label longer than max_label_length is truncated.
    void record_label(Handle object, std::string_view label);
    Status flush() noexcept;
    // Sequence of the next chunk to be submitted.
    std::uint64_t sequence() const noexcept;
    Status status() const noexcept;

private:
    void record_payload(Opcode opcode, const void* payload, std::uint32_t size);
};

} // namespace directmetal::bridge
