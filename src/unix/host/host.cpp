#include "calls.hpp"
#include "host_context.hpp"
#include "teardown.hpp"

#include <directmetal/host/dispatch.hpp>

#include <array>
#include <cstring>
#include <new>

namespace directmetal::host {
namespace {

using namespace bridge;

using Entry = Status (*)(HostContext& context, void* params);

// Runs Handler on a copy of the caller's parameters and copies them back only on success.
template <typename Params, Status (*Handler)(HostContext&, Params&)>
Status invoke(HostContext& context, void* params) {
    if (params == nullptr) {
        return status_invalid_parameter;
    }
    Params copy;
    std::memcpy(&copy, params, sizeof(copy));
    const Status status = Handler(context, copy);
    if (status >= 0) {
        std::memcpy(params, &copy, sizeof(copy));
    }
    return status;
}

struct Call {
    Entry entry;
    bool needs_initialize;
};

// Indexed by FunctionCode.
constexpr std::array<Call, to_index(FunctionCode::count)> calls = {{
    {invoke<QueryVersionParams, query_version>, false},
    {invoke<LogCallParams, log_call>, false},
    {invoke<InitializeParams, initialize>, false},
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

} // namespace

class Host::State {
public:
    explicit State(DeviceProvider& provider) noexcept : context(provider) {}

    // Devices still open are torn down as destroy_device would.
    ~State() {
        for (const TableEntry& device : context.table.owned_by({})) {
            destroy_device_objects(context, device.handle);
        }
    }

    HostContext context;
};

Host::Host(DeviceProvider& provider) : state_(std::make_unique<State>(provider)) {}

Host::~Host() = default;

// Calls before initialize fail with status_unsuccessful: the protocol has no dedicated status.
Status Host::call(std::uint32_t code, void* params) noexcept {
    if (code >= calls.size()) {
        return status_invalid_parameter;
    }
    const Call& call = calls[code];
    if (call.needs_initialize && !state_->context.is_initialized.load()) {
        return params == nullptr ? status_invalid_parameter : status_unsuccessful;
    }
    try {
        return call.entry(state_->context, params);
    } catch (const std::bad_alloc&) {
        return status_no_memory;
    }
}

} // namespace directmetal::host
