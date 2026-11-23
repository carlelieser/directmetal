#include "fake_bridge.hpp"

#include <directmetal/bridge/client.hpp>

#include <atomic>
#include <cstdlib>

namespace fake_bridge {
namespace {

using namespace directmetal::bridge;

std::uint64_t next_handle = 100;

Handle new_handle() {
    return Handle{next_handle++};
}

Status create_device(CreateDeviceParams& params) {
    ++state().devices_created;
    params.device = new_handle();
    params.capabilities = {};
    params.capabilities.family = directmetal::backend::GpuFamily::apple9;
    params.capabilities.has_bc = 1;
    params.capabilities.has_unified_memory = 1;
    params.capabilities.max_texture_2d = 16384;
    params.capabilities.max_texture_3d = 2048;
    params.capabilities.max_buffer_length = std::uint64_t{1} << 34;
    return status_success;
}

Status open_stream(OpenStreamParams& params) {
    state().control = from_address<StreamControl>(params.control_address);
    params.stream = new_handle();
    params.progress_timeline = new_handle();
    return status_success;
}

Status create_texture(CreateTextureParams& params) {
    if (state().create_texture_status != status_success) {
        return state().create_texture_status;
    }
    state().textures.push_back(params.desc);
    params.texture = new_handle();
    return status_success;
}

Status import_memory(ImportMemoryParams& params) {
    const bool is_aligned =
        params.address % shared_memory_alignment == 0 && params.size % shared_memory_alignment == 0;
    if (!is_aligned || params.size == 0) {
        return status_invalid_parameter;
    }
    state().imported_sizes.push_back(params.size);
    params.buffer = new_handle();
    params.is_zero_copy = 1;
    return status_success;
}

// Records every command of the chunk; a submit completes at once.
Status submit_stream(const SubmitStreamParams& params) {
    const auto* chunk = from_address<const std::byte>(params.chunk_address);
    ChunkHeader header;
    std::memcpy(&header, chunk, sizeof(header));
    std::uint32_t offset = sizeof(header);
    for (std::uint32_t index = 0; index < header.command_count; ++index) {
        CommandHeader command;
        std::memcpy(&command, chunk + offset, sizeof(command));
        const std::byte* payload = chunk + offset + sizeof(command);
        state().commands.push_back(
            {command.opcode, std::vector<std::byte>(payload, chunk + offset + command.size)});
        if (command.opcode == Opcode::submit) {
            ++state().submits;
            std::atomic_ref<std::uint64_t>(state().control->completed_progress)
                .store(static_cast<std::uint64_t>(state().submits));
        }
        offset += command.size;
    }
    std::atomic_ref<std::uint64_t>(state().control->retired_sequence).store(params.sequence);
    return status_success;
}

Status wait_timeline(const WaitTimelineParams& params) {
    if (state().control != nullptr && state().control->status < 0) {
        return status_device_removed;
    }
    const bool is_complete = params.value <= static_cast<std::uint64_t>(state().submits);
    return is_complete ? status_success : status_timeout;
}

template <typename Params> Params& as(void* params) {
    return *static_cast<Params*>(params);
}

Status dispatch(FunctionCode code, void* params) {
    switch (code) {
    case FunctionCode::initialize:
        return state().initialize_status;
    case FunctionCode::create_device:
        return create_device(as<CreateDeviceParams>(params));
    case FunctionCode::destroy_device:
        ++state().devices_destroyed;
        return status_success;
    case FunctionCode::open_stream:
        return open_stream(as<OpenStreamParams>(params));
    case FunctionCode::create_texture:
        return create_texture(as<CreateTextureParams>(params));
    case FunctionCode::import_memory:
        return import_memory(as<ImportMemoryParams>(params));
    case FunctionCode::release_memory:
        ++state().memory_releases;
        return status_success;
    case FunctionCode::submit_stream:
        return submit_stream(as<SubmitStreamParams>(params));
    case FunctionCode::wait_timeline:
        return wait_timeline(as<WaitTimelineParams>(params));
    default:
        return status_not_implemented;
    }
}

} // namespace

State& state() {
    static State instance;
    return instance;
}

void reset() {
    state() = State{};
}

void fail_stream() {
    state().control->status = status_invalid_parameter;
}

} // namespace fake_bridge

extern "C" {

directmetal::bridge::Status directmetal_bridge_call(std::uint32_t code, void* params) noexcept {
    return fake_bridge::dispatch(static_cast<directmetal::bridge::FunctionCode>(code), params);
}

void* directmetal_bridge_allocate_shared(std::uint64_t size) noexcept {
    const std::uint64_t alignment = directmetal::bridge::shared_memory_alignment;
    const std::uint64_t rounded = (size + alignment - 1) / alignment * alignment;
    void* memory = std::aligned_alloc(alignment, rounded);
    if (memory != nullptr) {
        std::memset(memory, 0, rounded);
        ++fake_bridge::state().shared_allocations;
    }
    return memory;
}

void directmetal_bridge_free_shared(void* memory, std::uint64_t) noexcept {
    ++fake_bridge::state().shared_frees;
    std::free(memory);
}
}
