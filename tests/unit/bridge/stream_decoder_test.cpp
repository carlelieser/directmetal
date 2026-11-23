#include "chunk_builder.hpp"
#include "sample_commands.hpp"

#include <directmetal/bridge/stream_decoder.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;

std::vector<std::byte> sample_chunk(std::uint64_t sequence = 1) {
    ChunkBuilder builder(sequence);
    builder.add_label(label_object, label_text);
    for_each_sample([&](const auto& sample) { builder.add(sample); });
    builder.add_nop();
    return builder.build();
}

TEST(StreamDecoder, DecodesEveryOpcode) {
    const GuardedChunk chunk(sample_chunk());
    StreamDecoder decoder;
    RecordingSink sink;
    ASSERT_EQ(decoder.decode(chunk.submission(), sink), status_success);
    ASSERT_EQ(sink.commands.size(), static_cast<std::size_t>(Opcode::count));
    expect_samples(sink.commands, 0);
    EXPECT_EQ(sink.commands.back().opcode(), Opcode::nop);
    EXPECT_FALSE(decoder.has_failed());
    EXPECT_EQ(decoder.next_sequence(), 2u);
}

TEST(StreamDecoder, DecodesLabelsUpToMaxLength) {
    const std::string longest(max_label_length, 'x');
    const GuardedChunk chunk(ChunkBuilder().add_label({3}, "").add_label({4}, longest).build());
    StreamDecoder decoder;
    RecordingSink sink;
    ASSERT_EQ(decoder.decode(chunk.submission(), sink), status_success);
    ASSERT_EQ(sink.commands.size(), 2u);
    EXPECT_EQ(sink.commands[0].label(), "");
    EXPECT_EQ(sink.commands[1].label(), longest);
}

TEST(StreamDecoder, DecodesConsecutiveChunks) {
    StreamDecoder decoder;
    RecordingSink sink;
    for (std::uint64_t sequence = 1; sequence <= 3; ++sequence) {
        const GuardedChunk chunk(sample_chunk(sequence));
        ASSERT_EQ(decoder.decode(chunk.submission(), sink), status_success) << sequence;
    }
    EXPECT_EQ(sink.commands.size(), 3 * static_cast<std::size_t>(Opcode::count));
}

TEST(StreamDecoder, SinkFailureIsStickyAndStopsTheChunk) {
    const GuardedChunk chunk(sample_chunk());
    StreamDecoder decoder;
    RecordingSink sink;
    sink.failing_index = 2;
    EXPECT_EQ(decoder.decode(chunk.submission(), sink), status_invalid_handle);
    EXPECT_EQ(sink.commands.size(), 3u);
    EXPECT_EQ(decoder.failure().opcode, static_cast<std::uint32_t>(Opcode::clear_texture));
    EXPECT_EQ(decoder.failure().sequence, 1u);

    const GuardedChunk next(sample_chunk(2));
    EXPECT_EQ(decoder.decode(next.submission(), sink), status_invalid_handle);
    EXPECT_EQ(sink.commands.size(), 3u);
}

// Each case corrupts a valid chunk or its submission.
struct Malformation {
    std::string name;
    std::function<void(std::vector<std::byte>&, SubmitStreamParams&)> corrupt;
    Status status;
    std::uint32_t opcode;
};

template <typename Field>
void write_at(std::vector<std::byte>& bytes, std::size_t offset, Field value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

// Offsets in sample_chunk(): the label command comes first, destroy_object second.
constexpr std::size_t first_command = sizeof(ChunkHeader);
constexpr std::size_t label_size = 32;
constexpr std::size_t second_command = first_command + label_size;
constexpr auto framing = framing_failure_opcode;
constexpr auto set_label = static_cast<std::uint32_t>(Opcode::set_label);
constexpr auto destroy_object = static_cast<std::uint32_t>(Opcode::destroy_object);

void set_chunk_size(std::vector<std::byte>& bytes, SubmitStreamParams& params, std::size_t size) {
    bytes.resize(size);
    params.chunk_size = static_cast<std::uint32_t>(size);
    write_at(bytes, offsetof(ChunkHeader, size), params.chunk_size);
}

const std::vector<Malformation> malformations = {
    {"TruncatedChunkHeader",
     [](auto&, auto& params) { params.chunk_size = sizeof(ChunkHeader) - 8; },
     status_invalid_parameter, framing},
    {"TruncatedCommand",
     [](auto& bytes, auto& params) { set_chunk_size(bytes, params, bytes.size() - 8); },
     status_invalid_parameter, framing},
    {"CommandCountBeyondChunk",
     [](auto& bytes, auto&) {
         write_at<std::uint32_t>(bytes, offsetof(ChunkHeader, command_count), 12);
     },
     status_invalid_parameter, framing},
    {"TrailingBytes",
     [](auto& bytes, auto& params) { set_chunk_size(bytes, params, bytes.size() + 8); },
     status_invalid_parameter, framing},
    {"MisalignedChunkAddress", [](auto&, auto& params) { params.chunk_address += 4; },
     status_invalid_parameter, framing},
    {"NullChunkAddress", [](auto&, auto& params) { params.chunk_address = 0; },
     status_invalid_parameter, framing},
    {"MisalignedChunkSize",
     [](auto& bytes, auto& params) { set_chunk_size(bytes, params, bytes.size() + 4); },
     status_invalid_parameter, framing},
    {"OversizeChunk", [](auto&, auto& params) { params.chunk_size = max_chunk_size + 8; },
     status_invalid_parameter, framing},
    {"SubmissionSequenceMismatch", [](auto&, auto& params) { params.sequence = 2; },
     status_invalid_parameter, framing},
    {"HeaderSequenceMismatch",
     [](auto& bytes, auto&) { write_at<std::uint64_t>(bytes, offsetof(ChunkHeader, sequence), 2); },
     status_invalid_parameter, framing},
    {"SubmissionSizeMismatch", [](auto&, auto& params) { params.chunk_size -= 8; },
     status_invalid_parameter, framing},
    {"OutOfOrderSequence",
     [](auto& bytes, auto& params) {
         params.sequence = 2;
         write_at<std::uint64_t>(bytes, offsetof(ChunkHeader, sequence), 2);
     },
     status_invalid_parameter, framing},
    {"SubmissionReserved", [](auto&, auto& params) { params.reserved = 1; },
     status_invalid_parameter, framing},
    {"WrongMagic",
     [](auto& bytes, auto&) { write_at<std::uint32_t>(bytes, offsetof(ChunkHeader, magic), 0); },
     status_invalid_parameter, framing},
    {"WrongVersion",
     [](auto& bytes, auto&) { write_at<std::uint16_t>(bytes, offsetof(ChunkHeader, version), 2); },
     status_revision_mismatch, framing},
    {"ChunkHeaderReserved",
     [](auto& bytes, auto&) { write_at<std::uint16_t>(bytes, offsetof(ChunkHeader, reserved), 1); },
     status_invalid_parameter, framing},
    {"UnknownOpcode",
     [](auto& bytes, auto&) { write_at<std::uint16_t>(bytes, second_command, 0x7777); },
     status_invalid_parameter, 0x7777},
    {"OpcodeCount", [](auto& bytes, auto&) { write_at(bytes, second_command, Opcode::count); },
     status_invalid_parameter, static_cast<std::uint32_t>(Opcode::count)},
    {"CommandHeaderReserved",
     [](auto& bytes, auto&) { write_at<std::uint16_t>(bytes, second_command + 2, 1); },
     status_invalid_parameter, destroy_object},
    {"MisalignedCommandSize",
     [](auto& bytes, auto&) { write_at<std::uint32_t>(bytes, second_command + 4, 12); },
     status_invalid_parameter, destroy_object},
    {"PayloadSizeMismatch",
     [](auto& bytes, auto&) { write_at(bytes, second_command, Opcode::signal_timeline); },
     status_invalid_parameter, static_cast<std::uint32_t>(Opcode::signal_timeline)},
    {"CommandSizeBeyondChunk",
     [](auto& bytes, auto&) { write_at<std::uint32_t>(bytes, second_command + 4, 0x10000); },
     status_invalid_parameter, destroy_object},
    // Larger than any command, but inside the chunk: the decoder must not copy it.
    {"CommandSizeBeyondLargestCommand",
     [](auto& bytes, auto& params) {
         set_chunk_size(bytes, params, max_chunk_size);
         write_at<std::uint32_t>(bytes, second_command + 4, max_chunk_size - second_command);
     },
     status_invalid_parameter, destroy_object},
    {"CommandSizeZero",
     [](auto& bytes, auto&) { write_at<std::uint32_t>(bytes, second_command + 4, 0); },
     status_invalid_parameter, destroy_object},
    {"LabelLongerThanMax",
     [](auto& bytes, auto&) {
         write_at<std::uint32_t>(bytes, first_command + 8 + offsetof(SetLabelCommand, length),
                                 max_label_length + 1);
     },
     status_invalid_parameter, set_label},
    {"LabelPaddingNotZero",
     [](auto& bytes, auto&) { write_at<std::uint8_t>(bytes, second_command - 1, 1); },
     status_invalid_parameter, set_label},
    {"PayloadReserved",
     [](auto& bytes, auto&) {
         write_at<std::uint32_t>(bytes, first_command + 8 + offsetof(SetLabelCommand, reserved), 1);
     },
     status_invalid_parameter, set_label},
};

class StreamDecoderRejects : public testing::TestWithParam<Malformation> {};

TEST_P(StreamDecoderRejects, WithStickyErrorAndExecutesNothing) {
    // Corruptions see this placeholder address; it is moved to the guarded copy afterwards.
    constexpr std::uint64_t placeholder = 0x100000;
    std::vector<std::byte> bytes = sample_chunk();
    SubmitStreamParams params = submission_for(bytes);
    params.chunk_address = placeholder;
    GetParam().corrupt(bytes, params);
    const GuardedChunk chunk(bytes);
    if (params.chunk_address != 0) {
        params.chunk_address += directmetal::bridge::to_address(chunk.data()) - placeholder;
    }

    StreamDecoder decoder;
    RecordingSink sink;
    EXPECT_EQ(decoder.decode(params, sink), GetParam().status);
    EXPECT_TRUE(sink.commands.empty());
    EXPECT_TRUE(decoder.has_failed());
    EXPECT_EQ(decoder.failure().status, GetParam().status);
    EXPECT_EQ(decoder.failure().opcode, GetParam().opcode);
    EXPECT_EQ(decoder.failure().sequence, params.sequence);

    const GuardedChunk valid(sample_chunk(decoder.next_sequence()));
    EXPECT_EQ(decoder.decode(valid.submission(), sink), GetParam().status);
    EXPECT_TRUE(sink.commands.empty());
}

INSTANTIATE_TEST_SUITE_P(Malformations, StreamDecoderRejects, testing::ValuesIn(malformations),
                         [](const auto& info) { return info.param.name; });

} // namespace
