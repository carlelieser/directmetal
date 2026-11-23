#include <directmetal/bridge/stream_decoder.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

namespace directmetal::bridge {
namespace {

constexpr std::size_t opcode_count = static_cast<std::size_t>(Opcode::count);

struct ReservedSpan {
    std::uint32_t offset;
    std::uint32_t size;
};

// Reserved bytes of a payload, relative to its start; unused entries have size 0.
using PayloadReserved = std::array<ReservedSpan, 2>;

constexpr std::size_t index_of(Opcode opcode) noexcept {
    return static_cast<std::size_t>(opcode);
}

constexpr std::array<PayloadReserved, opcode_count> make_payload_reserved() noexcept {
    std::array<PayloadReserved, opcode_count> table{};
    table[index_of(Opcode::set_label)] = {{
        {offsetof(SetLabelCommand, reserved), sizeof(SetLabelCommand::reserved)},
    }};
    table[index_of(Opcode::clear_texture)] = {{
        {offsetof(ClearTextureCommand, color) + offsetof(backend::ClearColor, reserved),
         sizeof(backend::ClearColor::reserved)},
        {offsetof(ClearTextureCommand, reserved), sizeof(ClearTextureCommand::reserved)},
    }};
    table[index_of(Opcode::clear_depth_stencil)] = {{
        {offsetof(ClearDepthStencilCommand, clear) + offsetof(backend::DepthStencilClear, reserved),
         sizeof(backend::DepthStencilClear::reserved)},
    }};
    table[index_of(Opcode::copy_texture)] = {{
        {offsetof(CopyTextureCommand, reserved0), sizeof(CopyTextureCommand::reserved0)},
        {offsetof(CopyTextureCommand, reserved1), sizeof(CopyTextureCommand::reserved1)},
    }};
    table[index_of(Opcode::submit)] = {{
        {offsetof(SubmitCommand, reserved), sizeof(SubmitCommand::reserved)},
    }};
    return table;
}

constexpr std::array<PayloadReserved, opcode_count> payload_reserved = make_payload_reserved();

bool is_zero(const std::byte* bytes, std::uint32_t size) noexcept {
    return std::all_of(bytes, bytes + size, [](std::byte value) { return value == std::byte{}; });
}

bool has_zero_reserved(Opcode opcode, const std::byte* payload) noexcept {
    const PayloadReserved& spans = payload_reserved[index_of(opcode)];
    return std::all_of(spans.begin(), spans.end(), [payload](const ReservedSpan& span) {
        return is_zero(payload + span.offset, span.size);
    });
}

// Only set_label carries inline data; its length field says how much.
std::uint32_t inline_length_of(Opcode opcode, const std::byte* payload) noexcept {
    if (!layout_of(opcode).has_inline_data) {
        return 0;
    }
    SetLabelCommand label{};
    std::memcpy(&label, payload, sizeof(label));
    return label.length;
}

// The decoder's only read of shared memory. The PE side may still write the chunk while it is
// copied; that race is accepted because every byte is validated after the copy and nothing reads
// the shared chunk again.
void copy_from_shared(std::byte* destination, std::uint64_t address, std::uint32_t size) noexcept {
    std::memcpy(destination, from_address<const std::byte>(address), size);
}

} // namespace

std::string_view DecodedCommand::label() const noexcept {
    if (opcode_ != Opcode::set_label) {
        return {};
    }
    return {reinterpret_cast<const char*>(bytes_.data() + sizeof(SetLabelCommand)), label_length_};
}

struct StreamDecoder::Chunk {
    const std::byte* data;
    std::uint32_t size;
    std::uint32_t command_count;
    std::uint64_t sequence;
};

Status StreamDecoder::decode(const SubmitStreamParams& submission, CommandSink& sink) noexcept {
    if (has_failed()) {
        return failure_.status;
    }
    if (const Status status = validate_submission(submission); status < 0) {
        return fail(status, framing_failure_opcode, submission.sequence);
    }
    if (!reserve_copy(submission.chunk_size)) {
        return fail(status_no_memory, framing_failure_opcode, submission.sequence);
    }
    copy_from_shared(copy_.data(), submission.chunk_address, submission.chunk_size);
    Chunk chunk{};
    if (const Status status = read_header(submission, chunk); status < 0) {
        return fail(status, framing_failure_opcode, submission.sequence);
    }
    if (const Status status = walk(chunk, nullptr); status < 0) {
        return status;
    }
    ++next_sequence_;
    return walk(chunk, &sink);
}

Status StreamDecoder::validate_submission(const SubmitStreamParams& submission) const noexcept {
    const bool has_valid_size =
        submission.chunk_size >= sizeof(ChunkHeader) && submission.chunk_size <= max_chunk_size;
    const bool is_aligned = submission.chunk_address % command_alignment == 0;
    const bool is_addressable = submission.chunk_address != 0 && is_aligned;
    if (!has_valid_size || !is_addressable || submission.reserved != 0) {
        return status_invalid_parameter;
    }
    // Checked before any pointer arithmetic on the address.
    constexpr std::uint64_t address_limit = std::numeric_limits<std::uintptr_t>::max();
    if (submission.chunk_address > address_limit - submission.chunk_size) {
        return status_invalid_parameter;
    }
    return submission.sequence == next_sequence_ ? status_success : status_invalid_parameter;
}

bool StreamDecoder::reserve_copy(std::uint32_t size) noexcept {
    try {
        if (copy_.size() < size) {
            copy_.resize(size);
        }
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

Status StreamDecoder::read_header(const SubmitStreamParams& submission, Chunk& chunk) noexcept {
    ChunkHeader header{};
    std::memcpy(&header, copy_.data(), sizeof(header));
    if (header.magic != chunk_magic || header.reserved != 0) {
        return status_invalid_parameter;
    }
    if (header.version != stream_version) {
        return status_revision_mismatch;
    }
    const bool is_submitted_chunk =
        header.sequence == submission.sequence && header.size == submission.chunk_size;
    if (!is_submitted_chunk || header.size % command_alignment != 0) {
        return status_invalid_parameter;
    }
    chunk = {copy_.data(), header.size, header.command_count, header.sequence};
    return status_success;
}

Status StreamDecoder::walk(const Chunk& chunk, CommandSink* sink) noexcept {
    DecodedCommand command;
    std::uint32_t offset = sizeof(ChunkHeader);
    for (std::uint32_t index = 0; index < chunk.command_count; ++index) {
        std::uint32_t opcode = framing_failure_opcode;
        Status status = read_command(chunk, offset, command, opcode);
        if (status >= 0 && sink != nullptr) {
            status = sink->execute(command);
        }
        if (status < 0) {
            return fail(status, opcode, chunk.sequence);
        }
    }
    if (offset != chunk.size) {
        return fail(status_invalid_parameter, framing_failure_opcode, chunk.sequence);
    }
    return status_success;
}

Status StreamDecoder::read_command(const Chunk& chunk, std::uint32_t& offset,
                                   DecodedCommand& command, std::uint32_t& opcode) noexcept {
    const std::uint32_t remaining = chunk.size - offset;
    if (remaining < sizeof(CommandHeader)) {
        return status_invalid_parameter;
    }
    CommandHeader header{};
    std::memcpy(&header, chunk.data + offset, sizeof(header));
    opcode = static_cast<std::uint32_t>(header.opcode);
    if (opcode >= opcode_count || header.reserved != 0) {
        return status_invalid_parameter;
    }
    // Alignment is checked with the payload size: command_size is always aligned.
    const bool is_in_chunk = header.size >= sizeof(CommandHeader) && header.size <= remaining;
    if (!is_in_chunk || header.size > max_command_size) {
        return status_invalid_parameter;
    }
    const std::uint32_t body_size = header.size - static_cast<std::uint32_t>(sizeof(header));
    std::memcpy(command.bytes_.data(), chunk.data + offset + sizeof(header), body_size);
    command.opcode_ = header.opcode;
    offset += header.size;
    return validate_payload(command, header.size);
}

Status StreamDecoder::validate_payload(DecodedCommand& command, std::uint32_t size) noexcept {
    const Opcode opcode = command.opcode_;
    const std::byte* payload = command.bytes_.data();
    const std::uint32_t inline_length = inline_length_of(opcode, payload);
    const bool has_valid_length =
        inline_length <= max_label_length && size == command_size(opcode, inline_length);
    if (!has_valid_length || !has_zero_reserved(opcode, payload)) {
        return status_invalid_parameter;
    }
    const std::uint32_t data_end = layout_of(opcode).payload_size + inline_length;
    const std::uint32_t body_size = size - static_cast<std::uint32_t>(sizeof(CommandHeader));
    if (!is_zero(payload + data_end, body_size - data_end)) {
        return status_invalid_parameter;
    }
    command.label_length_ = inline_length;
    const std::string_view label = command.label();
    return utf8_prefix_length(label) == label.size() ? status_success : status_invalid_parameter;
}

Status StreamDecoder::fail(Status status, std::uint32_t opcode, std::uint64_t sequence) noexcept {
    failure_ = {status, opcode, sequence};
    return status;
}

} // namespace directmetal::bridge
