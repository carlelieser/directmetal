#pragma once

#include "fake_backend.hpp"

#include <directmetal/bridge/command_codec.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>
#include <directmetal/host/dispatch.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace directmetal::host::testing {

// A chunk in 8-byte-aligned memory that outlives its decoding.
class Chunk {
public:
    explicit Chunk(std::uint64_t sequence) : sequence_(sequence) {}

    template <typename Command> Chunk& add(const Command& command) {
        return add_raw(bridge::opcode_of<Command>, &command, sizeof(Command));
    }
    Chunk& add_label(bridge::Handle object, std::string_view text);
    Chunk& add_raw(bridge::Opcode opcode, const void* payload, std::uint32_t size);
    // Writes the header; the chunk may then be submitted.
    Chunk& seal();

    bridge::SubmitStreamParams submission(bridge::Handle stream) const;
    std::byte* data() noexcept { return reinterpret_cast<std::byte*>(words_.data()); }

private:
    std::uint64_t sequence_;
    std::uint32_t command_count_ = 0;
    std::vector<std::uint64_t> words_ = std::vector<std::uint64_t>(3);
};

struct OpenedStream {
    bridge::Handle stream;
    bridge::Handle progress;
};

// A Host over the fake backend with helpers for the calls most tests make.
class HostTest : public ::testing::Test {
protected:
    HostTest() : provider(gpu), host(std::make_unique<Host>(provider)) {}

    // Completes outstanding fake GPU work, so tearing the host down never waits forever.
    void TearDown() override;

    // Through the table initialize(is_wow64) chose.
    template <typename Params> bridge::Status call(bridge::FunctionCode code, Params& params) {
        return host->call(bridge::to_index(code), &params, abi);
    }

    // Initializes through the WoW64 table when is_wow64; later calls use the same table.
    void initialize(bool is_wow64 = false);
    bridge::Handle create_device();
    bridge::Handle create_buffer(bridge::Handle device, std::uint64_t size);
    bridge::Handle create_texture(bridge::Handle device, const backend::TextureDesc& desc);
    bridge::Handle create_timeline(bridge::Handle device, std::uint64_t initial,
                                   std::uint64_t* mirror);
    OpenedStream open_stream(bridge::Handle device);
    bridge::Status submit(Chunk& chunk);
    bridge::Status wait(bridge::Handle timeline, std::uint64_t value, std::uint64_t timeout_ns = 0);

    // Waits until the stream has retired sequence or failed.
    bool wait_retired(std::uint64_t sequence);
    bool wait_failed();

    static backend::TextureDesc render_target_desc();

    // A chunk that outlives the host, so teardown may still decode it.
    Chunk& chunk(std::uint64_t sequence);
    // Zeroed memory aligned to shared_memory_alignment that outlives the host.
    std::span<std::byte> shared_memory(std::size_t size);

    // Declared before host: the host may use them until it is destroyed.
    FakeGpu gpu;
    FakeProvider provider;
    std::deque<Chunk> chunks;
    std::vector<std::unique_ptr<std::byte, void (*)(void*)>> memories;
    alignas(64) bridge::StreamControl control{};
    std::unique_ptr<Host> host;
    bridge::Handle device{};
    OpenedStream stream{};
    CallerAbi abi = CallerAbi::native;
};

// A device and an open stream, initialized.
class StreamTest : public HostTest {
protected:
    void SetUp() override;
};

} // namespace directmetal::host::testing
