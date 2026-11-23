#pragma once

#include <directmetal/bridge/command_codec.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

// The unix side's reader of the command stream. Each chunk is copied out of shared memory once,
// into a buffer the decoder owns, and only the copy is validated and executed, so the PE side
// cannot change a chunk after submitting it. The copy's framing is validated in full before any
// of its commands reaches the sink, so a malformed chunk executes nothing.
namespace directmetal::bridge {

// DecodeFailure::opcode when the chunk framing, not a command, is malformed.
inline constexpr std::uint32_t framing_failure_opcode = UINT32_MAX;

class DecodedCommand {
public:
    Opcode opcode() const noexcept { return opcode_; }

    // Command must be the payload type of opcode().
    template <typename Command> Command payload() const noexcept {
        static_assert(opcode_of<Command> != Opcode::count, "not a command payload type");
        assert(opcode_of<Command> == opcode_);
        Command command{};
        std::memcpy(&command, bytes_.data(), sizeof(Command));
        return command;
    }

    // set_label's UTF-8 text; empty for every other opcode.
    std::string_view label() const noexcept;

private:
    friend class StreamDecoder;

    Opcode opcode_ = Opcode::nop;
    std::uint32_t label_length_ = 0;
    alignas(8) std::array<std::byte, max_command_size - sizeof(CommandHeader)> bytes_{};
};

class CommandSink {
public:
    virtual ~CommandSink() = default;
    // A failure is sticky: no later command reaches the sink.
    virtual Status execute(const DecodedCommand& command) noexcept = 0;
};

// What StreamControl reports once a stream has failed.
struct DecodeFailure {
    Status status;
    // The raw opcode of the failing command, or framing_failure_opcode.
    std::uint32_t opcode;
    std::uint64_t sequence;
};

// One per stream, used by its consumer thread. Chunks carry sequences 1, 2, 3, ... in submission
// order.
class StreamDecoder {
public:
    // Validates submission and the chunk it names, then passes the chunk's commands to sink in
    // order. Returns status_success or the stream's first failure, which is sticky: once failed,
    // later chunks are not read.
    Status decode(const SubmitStreamParams& submission, CommandSink& sink) noexcept;

    bool has_failed() const noexcept { return failure_.status < 0; }
    // status_success until has_failed().
    const DecodeFailure& failure() const noexcept { return failure_; }
    std::uint64_t next_sequence() const noexcept { return next_sequence_; }

private:
    struct Chunk;

    Status validate_submission(const SubmitStreamParams& submission) const noexcept;
    bool reserve_copy(std::uint32_t size) noexcept;
    Status read_header(const SubmitStreamParams& submission, Chunk& chunk) noexcept;
    Status walk(const Chunk& chunk, CommandSink* sink) noexcept;
    static Status read_command(const Chunk& chunk, std::uint32_t& offset, DecodedCommand& command,
                               std::uint32_t& opcode) noexcept;
    static Status validate_payload(DecodedCommand& command, std::uint32_t size) noexcept;
    Status fail(Status status, std::uint32_t opcode, std::uint64_t sequence) noexcept;

    DecodeFailure failure_{status_success, 0, 0};
    std::uint64_t next_sequence_ = 1;
    // The current chunk, copied from shared memory; grows to the largest chunk seen.
    std::vector<std::byte> copy_;
};

} // namespace directmetal::bridge
