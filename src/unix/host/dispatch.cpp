#include <directmetal/host/dispatch.hpp>

namespace directmetal::host {
namespace {

// The Metal backend. Devices are created only when a control call asks for one.
class MetalDeviceProvider final : public DeviceProvider {
public:
    backend::Result<std::unique_ptr<backend::Device>>
    create(const backend::DeviceRequest& request) override {
        return backend::create_device(request);
    }

    std::vector<backend::Capabilities> adapters() override { return backend::enumerate_adapters(); }
};

// Never destroyed: stream threads are not joined while the process exits.
Host& process_host() {
    static Host* const host = new Host(*new MetalDeviceProvider);
    return *host;
}

} // namespace

bridge::Status dispatch(std::uint32_t code, void* params) noexcept {
    return process_host().call(code, params);
}

} // namespace directmetal::host
