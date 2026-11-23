#pragma once

#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/stream_decoder.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>

// One command of every payload type, with distinct field values.
namespace directmetal::bridge::test {

using backend::ClearAspect;
using backend::ClearKind;

inline constexpr Handle label_object{5};
inline constexpr std::string_view label_text = "probe";

struct SampleCommands {
    DestroyObjectCommand destroy_object{{7}};
    ClearTextureCommand clear_texture{
        {11},
        {1, 2, 0, 1},
        {{0x3F800000, 0x3F000000, 0, 0x3F800000}, ClearKind::float_value, {}},
        0};
    ClearDepthStencilCommand clear_depth_stencil{
        {12}, {0, 1, 3, 1}, {0.5F, 3, ClearAspect::depth, 0}};
    CopyBufferCommand copy_buffer{{13}, 16, {14}, 32, 64};
    CopyBufferToTextureCommand copy_buffer_to_texture{
        {{15}, 256, 64, 4096, {16}, 1, 2, {1, 2, 0}, {4, 4, 1}}};
    CopyTextureToBufferCommand copy_texture_to_buffer{
        {{20}, 512, 128, 8192, {21}, 0, 3, {0, 1, 0}, {8, 2, 1}}};
    CopyTextureCommand copy_texture{{17}, 0, 1,         {0, 0, 0}, 0,     {18},
                                    1,    0, {2, 2, 0}, {4, 4, 1}, {0, 0}};
    SignalTimelineCommand signal_timeline{{19}, 42};
    SubmitCommand submit{0};
};

inline const SampleCommands samples{};

// Visits every payload sample in a fixed order.
template <typename Visitor> void for_each_sample(Visitor&& visit) {
    visit(samples.destroy_object);
    visit(samples.clear_texture);
    visit(samples.clear_depth_stencil);
    visit(samples.copy_buffer);
    visit(samples.copy_buffer_to_texture);
    visit(samples.copy_texture_to_buffer);
    visit(samples.copy_texture);
    visit(samples.signal_timeline);
    visit(samples.submit);
}

template <typename Command> bool has_same_bytes(const Command& left, const Command& right) {
    return std::memcmp(&left, &right, sizeof(Command)) == 0;
}

// commands, from start, are the label followed by every payload sample.
inline void expect_samples(const std::vector<DecodedCommand>& commands, std::size_t start) {
    ASSERT_GE(commands.size(), start + 10);
    EXPECT_EQ(commands[start].opcode(), Opcode::set_label);
    EXPECT_EQ(commands[start].label(), label_text);
    EXPECT_EQ(commands[start].payload<SetLabelCommand>().object.value, label_object.value);
    std::size_t index = start + 1;
    for_each_sample([&](const auto& sample) {
        using Command = std::decay_t<decltype(sample)>;
        const DecodedCommand& command = commands[index++];
        ASSERT_EQ(command.opcode(), opcode_of<Command>);
        EXPECT_TRUE(has_same_bytes(command.payload<Command>(), sample))
            << "opcode " << static_cast<int>(opcode_of<Command>);
        EXPECT_TRUE(command.label().empty());
    });
}

} // namespace directmetal::bridge::test
