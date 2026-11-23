#include "host_fixture.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace directmetal::host::testing {
namespace {

using namespace bridge;

constexpr auto poll_limit = std::chrono::seconds(5);

template <typename Predicate> bool poll(Predicate is_done) {
    const auto deadline = std::chrono::steady_clock::now() + poll_limit;
    while (!is_done()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

} // namespace

Chunk& Chunk::add_label(Handle object, std::string_view text) {
    std::vector<std::byte> payload(sizeof(SetLabelCommand) + text.size());
    const SetLabelCommand label{object, static_cast<std::uint32_t>(text.size()), 0};
    std::memcpy(payload.data(), &label, sizeof(label));
    std::memcpy(payload.data() + sizeof(label), text.data(), text.size());
    return add_raw(Opcode::set_label, payload.data(), static_cast<std::uint32_t>(payload.size()));
}

Chunk& Chunk::add_raw(Opcode opcode, const void* payload, std::uint32_t size) {
    const std::uint32_t command_bytes = align_command(sizeof(CommandHeader) + size);
    const std::size_t offset = words_.size() * sizeof(std::uint64_t);
    words_.resize(words_.size() + command_bytes / sizeof(std::uint64_t));
    const CommandHeader header{opcode, 0, command_bytes};
    std::memcpy(data() + offset, &header, sizeof(header));
    std::memcpy(data() + offset + sizeof(header), payload, size);
    ++command_count_;
    return *this;
}

Chunk& Chunk::seal() {
    const auto size = static_cast<std::uint32_t>(words_.size() * sizeof(std::uint64_t));
    const ChunkHeader header{chunk_magic, stream_version, 0, sequence_, size, command_count_};
    std::memcpy(data(), &header, sizeof(header));
    return *this;
}

SubmitStreamParams Chunk::submission(Handle stream) const {
    const auto size = static_cast<std::uint32_t>(words_.size() * sizeof(std::uint64_t));
    return {stream, bridge::to_address(words_.data()), size, 0, sequence_};
}

Chunk& HostTest::chunk(std::uint64_t sequence) {
    return chunks.emplace_back(sequence);
}

std::span<std::byte> HostTest::shared_memory(std::size_t size) {
    void* memory = std::aligned_alloc(shared_memory_alignment, size);
    std::memset(memory, 0, size);
    memories.emplace_back(static_cast<std::byte*>(memory), std::free);
    return {static_cast<std::byte*>(memory), size};
}

void HostTest::TearDown() {
    gpu.is_auto_complete = true;
    gpu.complete_all();
    host.reset();
}

void HostTest::initialize(bool is_wow64) {
    abi = is_wow64 ? CallerAbi::wow64 : CallerAbi::native;
    InitializeParams params{0, 0, is_wow64 ? initialize_flag_wow64 : 0, 0, 0};
    ASSERT_EQ(call(FunctionCode::initialize, params), status_success);
}

Handle HostTest::create_device() {
    CreateDeviceParams params{};
    EXPECT_EQ(call(FunctionCode::create_device, params), status_success);
    return params.device;
}

Handle HostTest::create_buffer(Handle owner, std::uint64_t size) {
    CreateBufferParams params{owner, {size, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, params), status_success);
    return params.buffer;
}

Handle HostTest::create_texture(Handle owner, const backend::TextureDesc& desc) {
    CreateTextureParams params{owner, desc, 0, {}, 0};
    EXPECT_EQ(call(FunctionCode::create_texture, params), status_success);
    return params.texture;
}

Handle HostTest::create_timeline(Handle owner, std::uint64_t initial, std::uint64_t* mirror) {
    CreateTimelineParams params{owner, initial, bridge::to_address(mirror), {}};
    EXPECT_EQ(call(FunctionCode::create_timeline, params), status_success);
    return params.timeline;
}

OpenedStream HostTest::open_stream(Handle owner) {
    OpenStreamParams params{owner, bridge::to_address(&control), {}, {}};
    EXPECT_EQ(call(FunctionCode::open_stream, params), status_success);
    return {params.stream, params.progress_timeline};
}

Status HostTest::submit(Chunk& chunk) {
    SubmitStreamParams params = chunk.seal().submission(stream.stream);
    return call(FunctionCode::submit_stream, params);
}

Status HostTest::wait(Handle timeline, std::uint64_t value, std::uint64_t timeout_ns) {
    WaitTimelineParams params{timeline, value, timeout_ns};
    return call(FunctionCode::wait_timeline, params);
}

bool HostTest::wait_retired(std::uint64_t sequence) {
    return poll([&] {
        const auto retired =
            std::atomic_ref(control.retired_sequence).load(std::memory_order_acquire);
        return retired >= sequence || std::atomic_ref(control.status).load() < 0;
    });
}

bool HostTest::wait_failed() {
    return poll([&] { return std::atomic_ref(control.status).load() < 0; });
}

backend::TextureDesc HostTest::render_target_desc() {
    return {backend::TextureType::tex2d,
            backend::StorageMode::gpu_private,
            0,
            backend::PixelFormat::rgba8_unorm,
            {8, 8, 1},
            2,
            1,
            1,
            backend::TextureUsage::render_target | backend::TextureUsage::shader_read};
}

void StreamTest::SetUp() {
    initialize();
    device = create_device();
    stream = open_stream(device);
}

} // namespace directmetal::host::testing
