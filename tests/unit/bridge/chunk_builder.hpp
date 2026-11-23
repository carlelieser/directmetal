#pragma once

#include <directmetal/bridge/command_codec.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>
#include <directmetal/bridge/stream_decoder.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// Hand-built chunks for decoder tests, independent of StreamWriter.
namespace directmetal::bridge::test {

struct ByteRange {
    std::size_t begin;
    std::size_t end;
};

class ChunkBuilder {
public:
    explicit ChunkBuilder(std::uint64_t sequence = 1) : sequence_(sequence) {}

    template <typename Command> ChunkBuilder& add(const Command& command) {
        return add_raw(opcode_of<Command>, &command, sizeof(Command));
    }
    ChunkBuilder& add_nop();
    ChunkBuilder& add_label(Handle object, std::string_view text);
    // A command header of opcode followed by payload, padded with zeros to command_alignment.
    ChunkBuilder& add_raw(Opcode opcode, const void* payload, std::uint32_t payload_size);

    std::vector<std::byte> build() const;
    // Bytes the decoder passes through unchecked: payload fields other than reserved fields and
    // set_label's length. Label text is checked as UTF-8.
    const std::vector<ByteRange>& free_ranges() const noexcept { return free_ranges_; }

private:
    void add_free_payload(Opcode opcode, std::size_t payload_begin);

    std::uint64_t sequence_;
    std::uint32_t command_count_ = 0;
    std::vector<std::byte> commands_;
    std::vector<ByteRange> free_ranges_;
};

// The SubmitStreamParams matching a chunk as built.
SubmitStreamParams submission_for(std::span<const std::byte> chunk);

// Where GuardedChunk puts bytes in front of its inaccessible page.
enum class Placement {
    // The last byte is the last before the guard, so any read past the end faults; the start is
    // misaligned when the size is not a multiple of 8.
    at_guard,
    // The start is 8-byte aligned and up to 7 readable bytes separate the end from the guard.
    aligned,
};

// Copies bytes in front of an inaccessible page.
class GuardedChunk {
public:
    explicit GuardedChunk(std::span<const std::byte> bytes,
                          Placement placement = Placement::at_guard);
    GuardedChunk(const GuardedChunk&) = delete;
    GuardedChunk& operator=(const GuardedChunk&) = delete;
    ~GuardedChunk();

    std::byte* data() const noexcept { return start_; }
    // Submission whose address is this copy, with the sequence and size of the bytes' header.
    SubmitStreamParams submission() const;

private:
    std::byte* mapping_ = nullptr;
    std::size_t mapping_size_ = 0;
    std::byte* start_ = nullptr;
    std::size_t size_ = 0;
};

class RecordingSink : public CommandSink {
public:
    Status execute(const DecodedCommand& command) noexcept override;

    std::vector<DecodedCommand> commands;
    // execute returns failure_status for the command at this index.
    std::size_t failing_index = SIZE_MAX;
    Status failure_status = status_invalid_handle;
};

} // namespace directmetal::bridge::test
