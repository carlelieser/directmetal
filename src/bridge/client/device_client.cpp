#include "control_call.hpp"

#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/runtime/log.hpp>

#include <algorithm>
#include <array>
#include <atomic>

namespace directmetal::bridge {
namespace {

constexpr std::array log_levels = {log::Level::trace, log::Level::debug, log::Level::info,
                                   log::Level::warn, log::Level::error};

log::Level host_log_level() noexcept {
    const auto level = std::find_if(log_levels.begin(), log_levels.end(), [](log::Level item) {
        return log::is_enabled(log::Component::host, item);
    });
    return level == log_levels.end() ? log::Level::off : *level;
}

std::uint64_t to_timeout_ns(std::chrono::nanoseconds timeout) noexcept {
    if (timeout == std::chrono::nanoseconds::max()) {
        return timeout_infinite;
    }
    return timeout.count() <= 0 ? 0 : static_cast<std::uint64_t>(timeout.count());
}

} // namespace

backend::Status initialize(std::string_view client_name, bool is_wow64) noexcept {
    const std::size_t length = std::min<std::size_t>(client_name.size(), max_name_length);
    InitializeParams params{
        bridge::to_address(client_name.data()), static_cast<std::uint32_t>(length),
        is_wow64 ? initialize_flag_wow64 : 0, static_cast<std::uint32_t>(host_log_level()), 0};
    return invoke(FunctionCode::initialize, &params);
}

backend::Result<std::uint32_t> query_version() noexcept {
    QueryVersionParams params{};
    return invoke_for<std::uint32_t>(FunctionCode::query_version, params,
                                     [](const QueryVersionParams& out) { return out.version; });
}

backend::Result<std::vector<backend::Capabilities>> enumerate_adapters() {
    std::vector<backend::Capabilities> adapters;
    for (std::uint32_t index = 0;; ++index) {
        EnumerateAdaptersParams params{index, 0, {}};
        const Status status = call(FunctionCode::enumerate_adapters, &params);
        if (status == status_no_more_entries) {
            return backend::Result<std::vector<backend::Capabilities>>::success(
                std::move(adapters));
        }
        if (const backend::Status result = report(FunctionCode::enumerate_adapters, status);
            result != backend::Status::success) {
            return backend::Result<std::vector<backend::Capabilities>>::failure(result);
        }
        adapters.push_back(params.adapter);
    }
}

TimelineClient::TimelineClient(Handle timeline, std::uint64_t* mirror) noexcept
    : timeline_(timeline), mirror_(mirror) {}

Handle TimelineClient::handle() const noexcept {
    return timeline_;
}

std::uint64_t TimelineClient::completed() const noexcept {
    return std::atomic_ref<std::uint64_t>(*mirror_).load(std::memory_order_acquire);
}

backend::Status TimelineClient::wait(std::uint64_t value,
                                     std::chrono::nanoseconds timeout) const noexcept {
    if (completed() >= value) {
        return backend::Status::success;
    }
    WaitTimelineParams params{timeline_, value, to_timeout_ns(timeout)};
    return invoke(FunctionCode::wait_timeline, &params);
}

DeviceClient::DeviceClient(Handle device, const backend::Capabilities& capabilities) noexcept
    : device_(device), capabilities_(capabilities) {}

backend::Result<DeviceClient> DeviceClient::create(std::uint64_t registry_id,
                                                   const DeviceOptions& options) {
    CreateDeviceParams params{registry_id, options, {}, {}};
    return invoke_for<DeviceClient>(
        FunctionCode::create_device, params,
        [](const CreateDeviceParams& out) { return DeviceClient(out.device, out.capabilities); });
}

Handle DeviceClient::handle() const noexcept {
    return device_;
}

const backend::Capabilities& DeviceClient::capabilities() const noexcept {
    return capabilities_;
}

backend::Result<Handle> DeviceClient::create_buffer(const backend::BufferDesc& desc) {
    CreateBufferParams params{device_, desc, {}};
    return invoke_for<Handle>(FunctionCode::create_buffer, params,
                              [](const CreateBufferParams& out) { return out.buffer; });
}

backend::Result<ImportedMemory> DeviceClient::import_memory(std::span<std::byte> memory) {
    ImportMemoryParams params{device_, bridge::to_address(memory.data()), memory.size(), {}, 0, 0};
    return invoke_for<ImportedMemory>(FunctionCode::import_memory, params,
                                      [](const ImportMemoryParams& out) {
                                          return ImportedMemory{out.buffer, out.is_zero_copy != 0};
                                      });
}

backend::Status DeviceClient::release_memory(Handle buffer) noexcept {
    ReleaseMemoryParams params{buffer};
    return invoke(FunctionCode::release_memory, &params);
}

backend::Result<Handle> DeviceClient::create_texture(const backend::TextureDesc& desc) {
    CreateTextureParams params{device_, desc, 0, {}, 0};
    return invoke_for<Handle>(FunctionCode::create_texture, params,
                              [](const CreateTextureParams& out) { return out.texture; });
}

backend::Result<Handle> DeviceClient::create_timeline(std::uint64_t initial_value,
                                                      std::uint64_t& mirror) {
    CreateTimelineParams params{device_, initial_value, bridge::to_address(&mirror), {}};
    return invoke_for<Handle>(FunctionCode::create_timeline, params,
                              [](const CreateTimelineParams& out) { return out.timeline; });
}

void DeviceClient::destroy_timeline(Handle timeline) noexcept {
    DestroyTimelineParams params{timeline};
    invoke(FunctionCode::destroy_timeline, &params);
}

backend::Result<OpenedStream> DeviceClient::open_stream(StreamControl& control) {
    OpenStreamParams params{device_, bridge::to_address(&control), {}, {}};
    return invoke_for<OpenedStream>(FunctionCode::open_stream, params,
                                    [](const OpenStreamParams& out) {
                                        return OpenedStream{out.stream, out.progress_timeline};
                                    });
}

void DeviceClient::close_stream(Handle stream) noexcept {
    CloseStreamParams params{stream};
    invoke(FunctionCode::close_stream, &params);
}

void DeviceClient::destroy() noexcept {
    if (is_null(device_)) {
        return;
    }
    DestroyDeviceParams params{device_};
    invoke(FunctionCode::destroy_device, &params);
    device_ = {};
}

} // namespace directmetal::bridge
