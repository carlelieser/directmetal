#pragma once

#include <directmetal/bridge/commands.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

// Size rules of the command stream, shared by StreamWriter and StreamDecoder on top of
// opcode_layouts.
namespace directmetal::bridge {

inline constexpr std::uint32_t command_alignment = 8;

constexpr std::uint32_t align_command(std::uint32_t size) noexcept {
    return (size + command_alignment - 1) & ~(command_alignment - 1);
}

// Bytes of a command of opcode, including its header and inline_length bytes of inline data
// padded to command_alignment. inline_length is ignored for opcodes without inline data.
constexpr std::uint32_t command_size(Opcode opcode, std::uint32_t inline_length) noexcept {
    const OpcodeLayout& layout = layout_of(opcode);
    const std::uint32_t inline_size = layout.has_inline_data ? inline_length : 0;
    return align_command(static_cast<std::uint32_t>(sizeof(CommandHeader)) + layout.payload_size +
                         inline_size);
}

constexpr std::uint32_t compute_max_command_size() noexcept {
    std::uint32_t largest = 0;
    for (std::size_t index = 0; index < opcode_layouts.size(); ++index) {
        const std::uint32_t size = command_size(static_cast<Opcode>(index), max_label_length);
        largest = size > largest ? size : largest;
    }
    return largest;
}

inline constexpr std::uint32_t max_command_size = compute_max_command_size();
// The smallest chunk that holds any single command.
inline constexpr std::uint32_t min_chunk_size =
    align_command(static_cast<std::uint32_t>(sizeof(ChunkHeader))) + max_command_size;

// Bytes of the longest prefix of text that is well-formed UTF-8. Labels must be well-formed in
// full, so the host can pass them to Metal as strings.
std::size_t utf8_prefix_length(std::string_view text) noexcept;

static_assert(sizeof(ChunkHeader) % command_alignment == 0);
static_assert(command_size(Opcode::set_label, max_label_length) ==
              sizeof(CommandHeader) + sizeof(SetLabelCommand) + max_label_length);
static_assert(max_chunk_size % command_alignment == 0);

} // namespace directmetal::bridge
