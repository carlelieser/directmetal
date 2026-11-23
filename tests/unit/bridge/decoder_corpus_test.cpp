#include "chunk_builder.hpp"
#include "sample_commands.hpp"

#include <directmetal/bridge/stream_decoder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

// Deterministic replay of malformed chunks, standing in for a fuzzer: every chunk ends at an
// inaccessible page, so a read past the submitted size faults.
namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;

struct Corpus {
    std::vector<std::byte> bytes;
    std::vector<ByteRange> free_ranges;
};

Corpus sample_corpus() {
    ChunkBuilder builder;
    builder.add_label(label_object, label_text);
    for_each_sample([&](const auto& sample) { builder.add(sample); });
    builder.add_nop();
    return {builder.build(), builder.free_ranges()};
}

bool is_free(const Corpus& corpus, std::size_t offset) {
    return std::any_of(
        corpus.free_ranges.begin(), corpus.free_ranges.end(),
        [offset](const ByteRange& range) { return offset >= range.begin && offset < range.end; });
}

struct Outcome {
    Status status;
    std::size_t executed;
};

Outcome replay(const std::vector<std::byte>& bytes, std::uint32_t chunk_size,
               Placement placement = Placement::at_guard) {
    const GuardedChunk chunk(bytes, placement);
    const SubmitStreamParams params{
        {1}, directmetal::bridge::to_address(chunk.data()), chunk_size, 0, 1};
    StreamDecoder decoder;
    RecordingSink sink;
    const Status status = decoder.decode(params, sink);
    return {status, sink.commands.size()};
}

// Flipping any byte the decoder checks is rejected before anything executes; flipping a byte it
// passes through is accepted.
TEST(DecoderCorpus, SingleByteFlips) {
    const Corpus corpus = sample_corpus();
    const auto size = static_cast<std::uint32_t>(corpus.bytes.size());
    for (std::size_t offset = 0; offset < corpus.bytes.size(); ++offset) {
        for (const std::byte mask : {std::byte{0x80}, std::byte{0xFF}}) {
            std::vector<std::byte> bytes = corpus.bytes;
            bytes[offset] ^= mask;
            const Outcome outcome = replay(bytes, size);
            if (is_free(corpus, offset)) {
                EXPECT_EQ(outcome.status, status_success) << "offset " << offset;
            } else {
                EXPECT_LT(outcome.status, 0) << "offset " << offset;
                EXPECT_EQ(outcome.executed, 0u) << "offset " << offset;
            }
        }
    }
}

// Every proper prefix is rejected, whether or not the header's size was updated to match. Sizes
// that are not a multiple of 8 are also replayed from an aligned address, so the size check, not
// the address check, rejects them.
TEST(DecoderCorpus, Truncations) {
    const Corpus corpus = sample_corpus();
    for (std::uint32_t size = 0; size < corpus.bytes.size(); ++size) {
        std::vector<std::byte> bytes(corpus.bytes.begin(), corpus.bytes.begin() + size);
        EXPECT_LT(replay(bytes, size).status, 0) << "size " << size;
        if (size < sizeof(ChunkHeader)) {
            continue;
        }
        std::memcpy(bytes.data() + offsetof(ChunkHeader, size), &size, sizeof(size));
        for (const Placement placement : {Placement::at_guard, Placement::aligned}) {
            const Outcome outcome = replay(bytes, size, placement);
            EXPECT_LT(outcome.status, 0) << "patched size " << size;
            EXPECT_EQ(outcome.executed, 0u) << "patched size " << size;
        }
    }
}

// xorshift64: the same sequence on every run and platform.
class Random {
public:
    explicit Random(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return state_;
    }
    std::size_t below(std::size_t bound) { return static_cast<std::size_t>(next() % bound); }

private:
    std::uint64_t state_;
};

// Random multi-byte corruption and resizing: decoding never faults, a rejected chunk executes
// nothing, and the error stays sticky.
TEST(DecoderCorpus, RandomCorruption) {
    const Corpus corpus = sample_corpus();
    Random random(0x5EED'D1EC'7A11'0001);
    std::size_t rejected = 0;
    constexpr int case_count = 20000;
    for (int index = 0; index < case_count; ++index) {
        std::vector<std::byte> bytes = corpus.bytes;
        const std::size_t flips = 1 + random.below(4);
        for (std::size_t flip = 0; flip < flips; ++flip) {
            bytes[random.below(bytes.size())] = static_cast<std::byte>(random.next());
        }
        bytes.resize(random.below(4) == 0 ? random.below(bytes.size() + 64) : bytes.size());
        const auto size = static_cast<std::uint32_t>(bytes.size());
        const Placement placement = index % 2 == 0 ? Placement::at_guard : Placement::aligned;
        const Outcome outcome = replay(bytes, size, placement);
        if (outcome.status < 0) {
            ++rejected;
            EXPECT_EQ(outcome.executed, 0u) << "case " << index;
        }
    }
    EXPECT_GT(rejected, static_cast<std::size_t>(case_count / 2));
}

} // namespace
