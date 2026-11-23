#include "calls.hpp"
#include "host_context.hpp"
#include "objects.hpp"
#include "teardown.hpp"

#include <directmetal/host/dispatch.hpp>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

namespace directmetal::host {
namespace {

using namespace bridge;

using Entry = Status (*)(HostContext& context, void* params, CallerAbi abi);

// Runs handler on a copy of the caller's parameters and copies them back only on success.
template <typename Params, typename Handler> Status run_on_copy(void* params, Handler handler) {
    if (params == nullptr) {
        return status_invalid_parameter;
    }
    Params copy;
    std::memcpy(&copy, params, sizeof(copy));
    const Status status = handler(copy);
    if (status >= 0) {
        std::memcpy(params, &copy, sizeof(copy));
    }
    return status;
}

template <typename Params, Status (*Handler)(HostContext&, Params&)>
Status invoke(HostContext& context, void* params, CallerAbi) {
    return run_on_copy<Params>(params, [&context](Params& copy) { return Handler(context, copy); });
}

// For the calls allowed before initialize, which must check the caller's ABI themselves.
template <typename Params, Status (*Handler)(HostContext&, Params&, CallerAbi)>
Status invoke_with_abi(HostContext& context, void* params, CallerAbi abi) {
    return run_on_copy<Params>(
        params, [&context, abi](Params& copy) { return Handler(context, copy, abi); });
}

struct Call {
    Entry entry;
    bool needs_initialize;
};

// Indexed by FunctionCode.
constexpr std::array<Call, to_index(FunctionCode::count)> calls = {{
    {invoke<QueryVersionParams, query_version>, false},
    {invoke_with_abi<LogCallParams, log_call>, false},
    {invoke_with_abi<InitializeParams, initialize>, false},
    {invoke<EnumerateAdaptersParams, enumerate_adapters>, true},
    {invoke<CreateDeviceParams, create_device>, true},
    {invoke<DestroyDeviceParams, destroy_device>, true},
    {invoke<CreateBufferParams, create_buffer>, true},
    {invoke<ImportMemoryParams, import_memory>, true},
    {invoke<CreateTextureParams, create_texture>, true},
    {invoke<CreateTimelineParams, create_timeline>, true},
    {invoke<OpenStreamParams, open_stream>, true},
    {invoke<SubmitStreamParams, submit_stream>, true},
    {invoke<WaitTimelineParams, wait_timeline>, true},
    {invoke<CloseStreamParams, close_stream>, true},
    {invoke<ReleaseMemoryParams, release_memory>, true},
    {invoke<DestroyTimelineParams, destroy_timeline>, true},
}};

// No exception may cross the bridge; one the host cannot name leaves it in an unknown state, which
// is reported once rather than on every later call.
void report_unexpected_exception(std::uint32_t code) noexcept {
    static std::atomic<bool> has_reported = false;
    if (!has_reported.exchange(true)) {
        std::fprintf(stderr, "directmetal: host: control call %u threw an unexpected exception\n",
                     code);
    }
}

} // namespace

class Host::State {
public:
    explicit State(DeviceProvider& provider) noexcept : context(provider) {}

    // Devices still open are torn down as destroy_device would.
    ~State() {
        for (const TableEntry& device : context.table.owned_by({})) {
            if (mark_destroyed(*static_cast<DeviceObject*>(device.object.get()))) {
                destroy_device_objects(context, device.handle);
            }
        }
    }

    HostContext context;
};

Host::Host(DeviceProvider& provider) : state_(std::make_unique<State>(provider)) {}

Host::~Host() = default;

Status Host::call(std::uint32_t code, void* params, CallerAbi abi) noexcept {
    if (code >= calls.size()) {
        return status_invalid_parameter;
    }
    const Call& call = calls[code];
    HostContext& context = state_->context;
    if (call.needs_initialize && !context.is_initialized.load()) {
        return params == nullptr ? status_invalid_parameter : status_invalid_device_state;
    }
    const bool is_wow64_caller = abi == CallerAbi::wow64;
    if (call.needs_initialize && context.is_wow64.load() != is_wow64_caller) {
        return status_invalid_parameter;
    }
    try {
        return call.entry(context, params, abi);
    } catch (const std::bad_alloc&) {
        return status_no_memory;
    } catch (...) {
        report_unexpected_exception(code);
        return status_unsuccessful;
    }
}

} // namespace directmetal::host
