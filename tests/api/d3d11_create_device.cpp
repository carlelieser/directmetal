#include <windows.h>

#include <d3d11.h>

#include <cstdio>

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

} // namespace

int main() {
    HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
    if (d3d11 == nullptr) {
        std::fprintf(stderr, "LoadLibrary(d3d11.dll) failed: %lu\n", GetLastError());
        return 1;
    }
    const bool is_device_stubbed = check_create_device(d3d11);
    const bool is_swap_chain_stubbed = check_create_device_and_swap_chain(d3d11);
    FreeLibrary(d3d11);
    return is_device_stubbed && is_swap_chain_stubbed ? 0 : 1;
}
