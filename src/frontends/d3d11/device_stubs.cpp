#include "device_stubs.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::d3d11 {

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateBuffer(const D3D11_BUFFER_DESC*,
                                                    const D3D11_SUBRESOURCE_DATA*,
                                                    ID3D11Buffer**) noexcept {
    DM_STUB("ID3D11Device::CreateBuffer");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateTexture1D(const D3D11_TEXTURE1D_DESC*,
                                                       const D3D11_SUBRESOURCE_DATA*,
                                                       ID3D11Texture1D**) noexcept {
    DM_STUB("ID3D11Device::CreateTexture1D");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateTexture3D(const D3D11_TEXTURE3D_DESC*,
                                                       const D3D11_SUBRESOURCE_DATA*,
                                                       ID3D11Texture3D**) noexcept {
    DM_STUB("ID3D11Device::CreateTexture3D");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateShaderResourceView(
    ID3D11Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC*, ID3D11ShaderResourceView**) noexcept {
    DM_STUB("ID3D11Device::CreateShaderResourceView");
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CreateUnorderedAccessView(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC*,
                                       ID3D11UnorderedAccessView**) noexcept {
    DM_STUB("ID3D11Device::CreateUnorderedAccessView");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateDepthStencilView(ID3D11Resource*,
                                                              const D3D11_DEPTH_STENCIL_VIEW_DESC*,
                                                              ID3D11DepthStencilView**) noexcept {
    DM_STUB("ID3D11Device::CreateDepthStencilView");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC*, UINT,
                                                         const void*, SIZE_T,
                                                         ID3D11InputLayout**) noexcept {
    DM_STUB("ID3D11Device::CreateInputLayout");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateVertexShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                          ID3D11VertexShader**) noexcept {
    DM_STUB("ID3D11Device::CreateVertexShader");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateGeometryShader(const void*, SIZE_T,
                                                            ID3D11ClassLinkage*,
                                                            ID3D11GeometryShader**) noexcept {
    DM_STUB("ID3D11Device::CreateGeometryShader");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateGeometryShaderWithStreamOutput(
    const void*, SIZE_T, const D3D11_SO_DECLARATION_ENTRY*, UINT, const UINT*, UINT, UINT,
    ID3D11ClassLinkage*, ID3D11GeometryShader**) noexcept {
    DM_STUB("ID3D11Device::CreateGeometryShaderWithStreamOutput");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreatePixelShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                         ID3D11PixelShader**) noexcept {
    DM_STUB("ID3D11Device::CreatePixelShader");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateHullShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                        ID3D11HullShader**) noexcept {
    DM_STUB("ID3D11Device::CreateHullShader");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateDomainShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                          ID3D11DomainShader**) noexcept {
    DM_STUB("ID3D11Device::CreateDomainShader");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateComputeShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                           ID3D11ComputeShader**) noexcept {
    DM_STUB("ID3D11Device::CreateComputeShader");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateClassLinkage(ID3D11ClassLinkage**) noexcept {
    DM_STUB("ID3D11Device::CreateClassLinkage");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateBlendState(const D3D11_BLEND_DESC*,
                                                        ID3D11BlendState**) noexcept {
    DM_STUB("ID3D11Device::CreateBlendState");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC*,
                                                               ID3D11DepthStencilState**) noexcept {
    DM_STUB("ID3D11Device::CreateDepthStencilState");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateRasterizerState(const D3D11_RASTERIZER_DESC*,
                                                             ID3D11RasterizerState**) noexcept {
    DM_STUB("ID3D11Device::CreateRasterizerState");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateSamplerState(const D3D11_SAMPLER_DESC*,
                                                          ID3D11SamplerState**) noexcept {
    DM_STUB("ID3D11Device::CreateSamplerState");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateQuery(const D3D11_QUERY_DESC*,
                                                   ID3D11Query**) noexcept {
    DM_STUB("ID3D11Device::CreateQuery");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreatePredicate(const D3D11_QUERY_DESC*,
                                                       ID3D11Predicate**) noexcept {
    DM_STUB("ID3D11Device::CreatePredicate");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateCounter(const D3D11_COUNTER_DESC*,
                                                     ID3D11Counter**) noexcept {
    DM_STUB("ID3D11Device::CreateCounter");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateDeferredContext(UINT, ID3D11DeviceContext**) noexcept {
    DM_STUB("ID3D11Device::CreateDeferredContext");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::OpenSharedResource(HANDLE, REFIID, void**) noexcept {
    DM_STUB("ID3D11Device::OpenSharedResource");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckFormatSupport(DXGI_FORMAT, UINT*) noexcept {
    DM_STUB("ID3D11Device::CheckFormatSupport");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckMultisampleQualityLevels(DXGI_FORMAT, UINT,
                                                                     UINT*) noexcept {
    DM_STUB("ID3D11Device::CheckMultisampleQualityLevels");
}

void STDMETHODCALLTYPE DeviceStubs::CheckCounterInfo(D3D11_COUNTER_INFO*) noexcept {
    DM_STUB_VOID("ID3D11Device::CheckCounterInfo");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckCounter(const D3D11_COUNTER_DESC*, D3D11_COUNTER_TYPE*,
                                                    UINT*, LPSTR, UINT*, LPSTR, UINT*, LPSTR,
                                                    UINT*) noexcept {
    DM_STUB("ID3D11Device::CheckCounter");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckFeatureSupport(D3D11_FEATURE, void*, UINT) noexcept {
    DM_STUB("ID3D11Device::CheckFeatureSupport");
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE DeviceStubs::GetFeatureLevel() noexcept {
    DM_STUB_RETURN("ID3D11Device::GetFeatureLevel", D3D_FEATURE_LEVEL{});
}

UINT STDMETHODCALLTYPE DeviceStubs::GetCreationFlags() noexcept {
    DM_STUB_RETURN("ID3D11Device::GetCreationFlags", 0);
}

HRESULT STDMETHODCALLTYPE DeviceStubs::GetDeviceRemovedReason() noexcept {
    DM_STUB("ID3D11Device::GetDeviceRemovedReason");
}

HRESULT STDMETHODCALLTYPE DeviceStubs::SetExceptionMode(UINT) noexcept {
    DM_STUB("ID3D11Device::SetExceptionMode");
}

UINT STDMETHODCALLTYPE DeviceStubs::GetExceptionMode() noexcept {
    DM_STUB_RETURN("ID3D11Device::GetExceptionMode", 0);
}

} // namespace directmetal::d3d11
