#include "chunk_builder.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

namespace directmetal::bridge::test {
namespace {

constexpr std::size_t opcode_count = static_cast<std::size_t>(Opcode::count);

// Payload fields the decoder checks: reserved fields, which must be zero, and set_label's length.
struct CheckedField {
    std::size_t offset;
    std::size_t size;
};

std::vector<CheckedField> checked_fields_of(Opcode opcode) {
    switch (opcode) {
    case Opcode::set_label:
        return {{offsetof(SetLabelCommand, reserved), 4}, {offsetof(SetLabelCommand, length), 4}};
    case Opcode::clear_texture:
        return {{offsetof(ClearTextureCommand, color) + offsetof(backend::ClearColor, reserved), 3},
                {offsetof(ClearTextureCommand, reserved), 4}};
    case Opcode::clear_depth_stencil:
        return {{offsetof(ClearDepthStencilCommand, clear) +
                     offsetof(backend::DepthStencilClear, reserved),
                 2}};
    case Opcode::copy_texture:
        return {{offsetof(CopyTextureCommand, reserved0), 4},
                {offsetof(CopyTextureCommand, reserved1), 8}};
    case Opcode::submit:
        return {{0, 8}};
    default:
        return {};
    }
}

void append(std::vector<std::byte>& bytes, const void* source, std::size_t size) {
    const auto* first = static_cast<const std::byte*>(source);
    bytes.insert(bytes.end(), first, first + size);
}

} // namespace

ChunkBuilder& ChunkBuilder::add_nop() {
    return add_raw(Opcode::nop, nullptr, 0);
}

ChunkBuilder& ChunkBuilder::add_label(Handle object, std::string_view text) {
    const SetLabelCommand label{object, static_cast<std::uint32_t>(text.size()), 0};
    std::vector<std::byte> payload;
    append(payload, &label, sizeof(label));
    append(payload, text.data(), text.size());
    return add_raw(Opcode::set_label, payload.data(), static_cast<std::uint32_t>(payload.size()));
}

ChunkBuilder& ChunkBuilder::add_raw(Opcode opcode, const void* payload,
                                    std::uint32_t payload_size) {
    const CommandHeader header{
        opcode, 0, align_command(static_cast<std::uint32_t>(sizeof(CommandHeader)) + payload_size)};
    const std::size_t begin = commands_.size();
    append(commands_, &header, sizeof(header));
    if (payload_size != 0) {
        append(commands_, payload, payload_size);
    }
    commands_.resize(begin + header.size);
    if (static_cast<std::size_t>(opcode) < opcode_count) {
        add_free_payload(opcode, sizeof(ChunkHeader) + begin + sizeof(CommandHeader));
    }
    ++command_count_;
    return *this;
}

void ChunkBuilder::add_free_payload(Opcode opcode, std::size_t payload_begin) {
    const std::vector<CheckedField> reserved = checked_fields_of(opcode);
    std::size_t offset = 0;
    const std::size_t payload_size = layout_of(opcode).payload_size;
    while (offset < payload_size) {
        const auto field =
            std::find_if(reserved.begin(), reserved.end(), [offset](const CheckedField& range) {
                return offset >= range.offset && offset < range.offset + range.size;
            });
        if (field == reserved.end()) {
            free_ranges_.push_back({payload_begin + offset, payload_begin + offset + 1});
        }
        ++offset;
    }
}

std::vector<std::byte> ChunkBuilder::build() const {
    const auto size = static_cast<std::uint32_t>(sizeof(ChunkHeader) + commands_.size());
    const ChunkHeader header{chunk_magic, stream_version, 0, sequence_, size, command_count_};
    std::vector<std::byte> bytes;
    append(bytes, &header, sizeof(header));
    bytes.insert(bytes.end(), commands_.begin(), commands_.end());
    return bytes;
}

SubmitStreamParams submission_for(std::span<const std::byte> chunk) {
    ChunkHeader header{};
    std::memcpy(&header, chunk.data(), sizeof(header));
    return {{1}, bridge::to_address(chunk.data()), header.size, 0, header.sequence};
}

GuardedChunk::GuardedChunk(std::span<const std::byte> bytes) : size_(bytes.size()) {
    const auto page_size = static_cast<std::size_t>(getpagesize());
    const std::size_t aligned_size = (bytes.size() + 7) / 8 * 8;
    const std::size_t data_pages = (aligned_size + page_size - 1) / page_size + 1;
    mapping_size_ = (data_pages + 1) * page_size;
    void* mapping =
        mmap(nullptr, mapping_size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) {
        throw std::runtime_error("GuardedChunk: mmap failed");
    }
    mapping_ = static_cast<std::byte*>(mapping);
    std::byte* guard = mapping_ + data_pages * page_size;
    if (mprotect(guard, page_size, PROT_NONE) != 0) {
        throw std::runtime_error("GuardedChunk: mprotect failed");
    }
    start_ = guard - aligned_size;
    std::memcpy(start_, bytes.data(), bytes.size());
}

GuardedChunk::~GuardedChunk() {
    munmap(mapping_, mapping_size_);
}

SubmitStreamParams GuardedChunk::submission() const {
    return submission_for({start_, size_});
}

Status RecordingSink::execute(const DecodedCommand& command) noexcept {
    const bool is_failing = commands.size() == failing_index;
    commands.push_back(command);
    return is_failing ? failure_status : status_success;
}

} // namespace directmetal::bridge::test
