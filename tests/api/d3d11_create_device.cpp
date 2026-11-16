#include <windows.h>

#include <d3d11.h>

#include <directmetal/bridge/protocol.hpp>

#include <cstdio>
#include <cstring>
#include <string_view>

namespace {

template <typename Function> Function find_export(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(module, name)));
}

bool is_not_implemented(const char* name, HRESULT result) {
    if (result == E_NOTIMPL) {
        return true;
    }
    std::fprintf(stderr, "%s returned 0x%08lx, expected E_NOTIMPL\n", name,
                 static_cast<unsigned long>(result));
    return false;
}

bool check_create_device(HMODULE d3d11) {
    auto create = find_export<PFN_D3D11_CREATE_DEVICE>(d3d11, "D3D11CreateDevice");
    if (create == nullptr) {
        std::fprintf(stderr, "d3d11.dll does not export D3D11CreateDevice\n");
        return false;
    }
    ID3D11Device* device = nullptr;
    const HRESULT result = create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                  D3D11_SDK_VERSION, &device, nullptr, nullptr);
    return is_not_implemented("D3D11CreateDevice", result);
}

bool check_create_device_and_swap_chain(HMODULE d3d11) {
    auto create =
        find_export<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(d3d11, "D3D11CreateDeviceAndSwapChain");
    if (create == nullptr) {
        std::fprintf(stderr, "d3d11.dll does not export D3D11CreateDeviceAndSwapChain\n");
        return false;
    }
    ID3D11Device* device = nullptr;
    const HRESULT result = create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                  D3D11_SDK_VERSION, nullptr, nullptr, &device, nullptr, nullptr);
    return is_not_implemented("D3D11CreateDeviceAndSwapChain", result);
}

using BridgeCall = directmetal::bridge::Status(__cdecl*)(std::uint32_t code, void* params);

// directmetal_bridge_call is the bridge module's public export, so this tests a public interface.
// Calls log_call directly. With the unix side present only directmetal.so can fill host_system;
// without it, the bridge must return Wine's unix library load failure.
bool check_bridge_reply(bool is_unix_side_expected) {
    using namespace directmetal::bridge;
    HMODULE bridge = GetModuleHandleW(L"directmetal.dll");
    auto call =
        bridge == nullptr ? nullptr : find_export<BridgeCall>(bridge, "directmetal_bridge_call");
    if (call == nullptr) {
        std::fprintf(stderr, "directmetal.dll is not loaded or lacks directmetal_bridge_call\n");
        return false;
    }
    constexpr std::string_view name = "d3d11_create_device";
    LogCallParams params{to_address(name.data()), static_cast<std::uint32_t>(name.size()), 0, {}};
    const Status status = call(to_index(FunctionCode::log_call), &params);
    const Status expected_status = is_unix_side_expected ? status_success : status_dll_not_found;
    const char* expected_host = is_unix_side_expected ? "Darwin" : "";
    if (status == expected_status && std::strcmp(params.host_system, expected_host) == 0) {
        return true;
    }
    std::fprintf(stderr,
                 "log_call returned 0x%08x with host_system \"%.*s\", expected 0x%08x \"%s\"\n",
                 static_cast<unsigned>(status), static_cast<int>(sizeof(params.host_system)),
                 params.host_system, static_cast<unsigned>(expected_status), expected_host);
    return false;
}

} // namespace

// Usage: d3d11_create_device.exe unix|no-unix
int main(int argc, char** argv) {
    const bool has_mode = argc == 2;
    const bool is_unix_side_expected = has_mode && std::strcmp(argv[1], "unix") == 0;
    if (!has_mode || !(is_unix_side_expected || std::strcmp(argv[1], "no-unix") == 0)) {
        std::fprintf(stderr, "usage: d3d11_create_device unix|no-unix\n");
        return 2;
    }
    HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
    if (d3d11 == nullptr) {
        std::fprintf(stderr, "LoadLibrary(d3d11.dll) failed: %lu\n", GetLastError());
        return 1;
    }
    const bool is_device_stubbed = check_create_device(d3d11);
    const bool is_swap_chain_stubbed = check_create_device_and_swap_chain(d3d11);
    const bool is_reply_expected = check_bridge_reply(is_unix_side_expected);
    FreeLibrary(d3d11);
    return is_device_stubbed && is_swap_chain_stubbed && is_reply_expected ? 0 : 1;
}
