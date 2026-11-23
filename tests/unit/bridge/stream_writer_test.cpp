#include "chunk_builder.hpp"
#include "fake_transport.hpp"
#include "sample_commands.hpp"

#include <directmetal/bridge/command_codec.hpp>
#include <directmetal/bridge/stream_decoder.hpp>
#include <directmetal/bridge/stream_writer.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;

constexpr Handle stream{0x51};

// Plays the unix side of one stream: decodes every submitted chunk and retires it.
class StreamWriterTest : public testing::Test {
protected:
    void SetUp() override {
        reset_shared_memory_stats();
        set_call_handler([this](FunctionCode code, void* params) { return on_call(code, params); });
    }

    void TearDown() override {
        set_call_handler({});
        fail_shared_allocations(false);
    }

    Status on_call(FunctionCode code, void* params) {
        if (code != FunctionCode::submit_stream) {
            return status_not_implemented;
        }
        const auto& submission = *static_cast<SubmitStreamParams*>(params);
        submissions.push_back(submission);
        if (submit_status < 0) {
            return submit_status;
        }
        if (const Status status = decoder.decode(submission, sink); status < 0) {
            std::atomic_ref<Status>(control.status).store(status);
        }
        if (is_retiring) {
            std::atomic_ref<std::uint64_t>(control.retired_sequence).store(submission.sequence);
        }
        return status_success;
    }

    StreamControl control{};
    StreamDecoder decoder;
    RecordingSink sink;
    std::vector<SubmitStreamParams> submissions;
    bool is_retiring = true;
    Status submit_status = status_success;
};

TEST_F(StreamWriterTest, RoundTripsEveryOpcode) {
    StreamWriter writer(stream, control);
    writer.record_label(label_object, label_text);
    for_each_sample([&](const auto& sample) { writer.record(sample); });
    ASSERT_EQ(submissions.size(), 1u) << "recording submit flushes";
    EXPECT_EQ(submissions[0].stream.value, stream.value);
    EXPECT_EQ(submissions[0].sequence, 1u);
    EXPECT_EQ(writer.status(), status_success);
    expect_samples(sink.commands, 0);
    EXPECT_EQ(sink.commands.size(), 10u);
}

TEST_F(StreamWriterTest, SubmitsOnlyOnFlush) {
    StreamWriter writer(stream, control);
    writer.record(samples.destroy_object);
    writer.record(samples.copy_buffer);
    EXPECT_TRUE(submissions.empty());
    EXPECT_EQ(writer.flush(), status_success);
    ASSERT_EQ(submissions.size(), 1u);
    EXPECT_EQ(submissions[0].chunk_size, sizeof(ChunkHeader) + 16 + 48);
    EXPECT_EQ(writer.flush(), status_success);
    EXPECT_EQ(submissions.size(), 1u) << "an empty chunk is not submitted";
    EXPECT_EQ(writer.sequence(), 2u);
}

TEST_F(StreamWriterTest, FlushesFullChunksInOrder) {
    StreamWriter writer(stream, control, min_chunk_size);
    constexpr std::uint64_t count = 200;
    for (std::uint64_t value = 1; value <= count; ++value) {
        writer.record(DestroyObjectCommand{{value}});
    }
    writer.flush();
    ASSERT_GT(submissions.size(), 1u);
    for (std::size_t index = 0; index < submissions.size(); ++index) {
        EXPECT_EQ(submissions[index].sequence, index + 1);
        EXPECT_LE(submissions[index].chunk_size, min_chunk_size);
    }
    EXPECT_GT(submissions[0].chunk_size + 16, min_chunk_size)
        << "a chunk is flushed only when full";
    ASSERT_EQ(sink.commands.size(), count);
    for (std::uint64_t value = 1; value <= count; ++value) {
        EXPECT_EQ(sink.commands[value - 1].payload<DestroyObjectCommand>().object.value, value);
    }
}

TEST_F(StreamWriterTest, ReusesRetiredChunks) {
    {
        StreamWriter writer(stream, control);
        for (int index = 0; index < 8; ++index) {
            writer.record(samples.destroy_object);
            writer.flush();
        }
        EXPECT_EQ(shared_memory_stats().total, 1u);
    }
    EXPECT_EQ(shared_memory_stats().live, 0u);
    EXPECT_EQ(shared_memory_stats().mismatched_frees, 0u);
}

TEST_F(StreamWriterTest, KeepsUnretiredChunks) {
    is_retiring = false;
    StreamWriter writer(stream, control);
    for (int index = 0; index < 4; ++index) {
        writer.record(samples.destroy_object);
        writer.flush();
    }
    std::set<std::uint64_t> addresses;
    for (const SubmitStreamParams& submission : submissions) {
        addresses.insert(submission.chunk_address);
    }
    EXPECT_EQ(addresses.size(), 4u);

    std::atomic_ref<std::uint64_t>(control.retired_sequence).store(2);
    writer.record(samples.destroy_object);
    writer.flush();
    EXPECT_TRUE(addresses.contains(submissions.back().chunk_address));
    EXPECT_EQ(shared_memory_stats().total, 4u);
}

TEST_F(StreamWriterTest, ZeroesPaddingOfReusedChunks) {
    StreamWriter writer(stream, control);
    writer.record_label(label_object, "abcdefg");
    writer.flush();
    writer.record_label(label_object, "a");
    EXPECT_EQ(writer.flush(), status_success);
    ASSERT_EQ(sink.commands.size(), 2u);
    EXPECT_EQ(sink.commands[1].label(), "a");
}

TEST_F(StreamWriterTest, TruncatesLongLabelsAtCodePoints) {
    StreamWriter writer(stream, control);
    writer.record_label(label_object, std::string(max_label_length + 10, 'x'));
    // U+00E9 is two bytes; the last one would start at max_label_length - 1.
    std::string accented(max_label_length - 1, 'y');
    accented += "\xC3\xA9";
    writer.record_label(label_object, accented);
    writer.flush();
    ASSERT_EQ(sink.commands.size(), 2u);
    EXPECT_EQ(sink.commands[0].label(), std::string(max_label_length, 'x'));
    EXPECT_EQ(sink.commands[1].label(), std::string(max_label_length - 1, 'y'));
}

TEST_F(StreamWriterTest, ReportsStickyStreamErrors) {
    StreamWriter writer(stream, control);
    writer.record(samples.destroy_object);
    writer.flush();
    std::atomic_ref<Status>(control.status).store(status_invalid_handle);
    EXPECT_EQ(writer.status(), status_invalid_handle);
    writer.record(samples.destroy_object);
    EXPECT_EQ(writer.flush(), status_invalid_handle);
    EXPECT_EQ(submissions.size(), 1u);
}

TEST_F(StreamWriterTest, DropsCommandsRecordedBeforeAStreamError) {
    StreamWriter writer(stream, control);
    writer.record(samples.destroy_object);
    std::atomic_ref<Status>(control.status).store(status_invalid_handle);
    EXPECT_EQ(writer.flush(), status_invalid_handle);
    EXPECT_TRUE(submissions.empty());
}

TEST_F(StreamWriterTest, MakesSubmitFailuresSticky) {
    submit_status = status_invalid_handle;
    StreamWriter writer(stream, control);
    writer.record(samples.destroy_object);
    EXPECT_EQ(writer.flush(), status_invalid_handle);
    writer.record(samples.destroy_object);
    EXPECT_EQ(writer.flush(), status_invalid_handle);
    EXPECT_EQ(submissions.size(), 1u);
}

TEST_F(StreamWriterTest, RejectsInvalidChunkSizes) {
    for (const std::uint32_t size : {min_chunk_size - 8, min_chunk_size + 4, max_chunk_size + 8}) {
        StreamWriter writer(stream, control, size);
        writer.record(samples.destroy_object);
        EXPECT_EQ(writer.flush(), status_invalid_parameter) << "size " << size;
    }
    EXPECT_TRUE(submissions.empty());
    EXPECT_EQ(shared_memory_stats().total, 0u);
}

TEST_F(StreamWriterTest, ReportsExhaustedSharedMemory) {
    fail_shared_allocations(true);
    StreamWriter writer(stream, control);
    writer.record(samples.destroy_object);
    EXPECT_EQ(writer.status(), status_no_memory);
    EXPECT_EQ(writer.flush(), status_no_memory);
    EXPECT_TRUE(submissions.empty());
}

} // namespace
