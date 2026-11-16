#include <directmetal/frontends/markers.hpp>

#include <d3d11.h>

extern "C" HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
                                            const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**,
                                            D3D_FEATURE_LEVEL*, ID3D11DeviceContext**) {
    DM_STUB("D3D11CreateDevice");
}

extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE,
                                                        UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
                                                        const DXGI_SWAP_CHAIN_DESC*,
                                                        IDXGISwapChain**, ID3D11Device**,
                                                        D3D_FEATURE_LEVEL*, ID3D11DeviceContext**) {
    DM_STUB("D3D11CreateDeviceAndSwapChain");
}
