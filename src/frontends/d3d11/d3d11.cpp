#include "bridge_device.hpp"
#include "device.hpp"
#include "feature_level.hpp"
#include "hresult.hpp"

#include <directmetal/com/boundary.hpp>
#include <directmetal/frontends/markers.hpp>

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <span>

namespace directmetal::d3d11 {
namespace {

struct DriverRule {
    D3D_DRIVER_TYPE type;
    // With a null adapter and, except for D3D_DRIVER_TYPE_SOFTWARE, a null software module.
    HRESULT result;
};

// Only hardware devices exist. Without an adapter, D3D_DRIVER_TYPE_UNKNOWN is invalid and
// D3D_DRIVER_TYPE_SOFTWARE needs a software module.
constexpr std::array driver_rules = {
    DriverRule{D3D_DRIVER_TYPE_HARDWARE, S_OK},
    DriverRule{D3D_DRIVER_TYPE_UNKNOWN, E_INVALIDARG},
    DriverRule{D3D_DRIVER_TYPE_REFERENCE, DXGI_ERROR_UNSUPPORTED},
    DriverRule{D3D_DRIVER_TYPE_NULL, DXGI_ERROR_UNSUPPORTED},
    DriverRule{D3D_DRIVER_TYPE_SOFTWARE, DXGI_ERROR_UNSUPPORTED},
    DriverRule{D3D_DRIVER_TYPE_WARP, DXGI_ERROR_UNSUPPORTED},
};

HRESULT check_driver(D3D_DRIVER_TYPE type, HMODULE software, UINT sdk_version) noexcept {
    const bool is_software = type == D3D_DRIVER_TYPE_SOFTWARE;
    if (is_software != (software != nullptr) || sdk_version != D3D11_SDK_VERSION) {
        return E_INVALIDARG;
    }
    const auto rule = std::find_if(driver_rules.begin(), driver_rules.end(),
                                   [type](const DriverRule& item) { return item.type == type; });
    return rule == driver_rules.end() ? E_INVALIDARG : rule->result;
}

HRESULT open_device(const DeviceSettings& settings, Device*& device) {
    auto bridge = BridgeDevice::open();
    if (!bridge.has_value()) {
        return to_hresult(bridge.status(), DXGI_ERROR_UNSUPPORTED);
    }
    return Device::create(std::move(bridge).value(), settings, device);
}

// Hands the new device's reference to the application, or releases it when not requested.
HRESULT publish(Device& device, ID3D11Device** device_out, ID3D11DeviceContext** context_out) {
    if (context_out != nullptr) {
        device.GetImmediateContext(context_out);
    }
    if (device_out != nullptr) {
        *device_out = &device;
    } else {
        device.Release();
    }
    const bool is_output_requested = device_out != nullptr || context_out != nullptr;
    return is_output_requested ? S_OK : S_FALSE;
}

} // namespace
} // namespace directmetal::d3d11

using namespace directmetal::d3d11;

// DXGI is absent in Phase 1, so an explicit adapter is rejected.
extern "C" HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType,
                                            HMODULE Software, UINT Flags,
                                            const D3D_FEATURE_LEVEL* pFeatureLevels,
                                            UINT FeatureLevels, UINT SDKVersion,
                                            ID3D11Device** ppDevice,
                                            D3D_FEATURE_LEVEL* pFeatureLevel,
                                            ID3D11DeviceContext** ppImmediateContext) {
    DM_IMPL("D3D11CreateDevice");
    if (ppDevice != nullptr) {
        *ppDevice = nullptr;
    }
    if (ppImmediateContext != nullptr) {
        *ppImmediateContext = nullptr;
    }
    const bool is_levels_valid = pFeatureLevels != nullptr || FeatureLevels == 0;
    if (pAdapter != nullptr || !is_levels_valid) {
        return E_INVALIDARG;
    }
    D3D_FEATURE_LEVEL level{};
    HRESULT result = check_driver(DriverType, Software, SDKVersion);
    if (SUCCEEDED(result)) {
        result = negotiate_feature_level(std::span(pFeatureLevels, FeatureLevels), level);
    }
    Device* device = nullptr;
    if (SUCCEEDED(result)) {
        result = directmetal::com::guard("D3D11CreateDevice",
                                         [&] { return open_device({level, Flags}, device); });
    }
    if (FAILED(result)) {
        return result;
    }
    if (pFeatureLevel != nullptr) {
        *pFeatureLevel = level;
    }
    return publish(*device, ppDevice, ppImmediateContext);
}

extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE,
                                                        UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
                                                        const DXGI_SWAP_CHAIN_DESC*,
                                                        IDXGISwapChain**, ID3D11Device**,
                                                        D3D_FEATURE_LEVEL*, ID3D11DeviceContext**) {
    DM_STUB("D3D11CreateDeviceAndSwapChain");
}
