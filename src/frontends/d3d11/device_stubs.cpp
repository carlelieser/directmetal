#include "device_stubs.hpp"

#include "stub_outputs.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::d3d11 {

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateBuffer(const D3D11_BUFFER_DESC*,
                                                    const D3D11_SUBRESOURCE_DATA*,
                                                    ID3D11Buffer** ppBuffer) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateBuffer", stub::clear_outputs(ppBuffer));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateTexture1D(const D3D11_TEXTURE1D_DESC*,
                                                       const D3D11_SUBRESOURCE_DATA*,
                                                       ID3D11Texture1D** ppTexture1D) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateTexture1D", stub::clear_outputs(ppTexture1D));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateTexture3D(const D3D11_TEXTURE3D_DESC*,
                                                       const D3D11_SUBRESOURCE_DATA*,
                                                       ID3D11Texture3D** ppTexture3D) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateTexture3D", stub::clear_outputs(ppTexture3D));
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CreateShaderResourceView(ID3D11Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC*,
                                      ID3D11ShaderResourceView** ppSRView) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateShaderResourceView", stub::clear_outputs(ppSRView));
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CreateUnorderedAccessView(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC*,
                                       ID3D11UnorderedAccessView** ppUAView) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateUnorderedAccessView", stub::clear_outputs(ppUAView));
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CreateDepthStencilView(ID3D11Resource*, const D3D11_DEPTH_STENCIL_VIEW_DESC*,
                                    ID3D11DepthStencilView** ppDepthStencilView) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateDepthStencilView", stub::clear_outputs(ppDepthStencilView));
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC*, UINT, const void*, SIZE_T,
                               ID3D11InputLayout** ppInputLayout) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateInputLayout", stub::clear_outputs(ppInputLayout));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateVertexShader(
    const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader** ppVertexShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateVertexShader", stub::clear_outputs(ppVertexShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateGeometryShader(
    const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11GeometryShader** ppGeometryShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateGeometryShader", stub::clear_outputs(ppGeometryShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateGeometryShaderWithStreamOutput(
    const void*, SIZE_T, const D3D11_SO_DECLARATION_ENTRY*, UINT, const UINT*, UINT, UINT,
    ID3D11ClassLinkage*, ID3D11GeometryShader** ppGeometryShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateGeometryShaderWithStreamOutput",
                   stub::clear_outputs(ppGeometryShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreatePixelShader(
    const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader** ppPixelShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreatePixelShader", stub::clear_outputs(ppPixelShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateHullShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                        ID3D11HullShader** ppHullShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateHullShader", stub::clear_outputs(ppHullShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateDomainShader(
    const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11DomainShader** ppDomainShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateDomainShader", stub::clear_outputs(ppDomainShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateComputeShader(
    const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11ComputeShader** ppComputeShader) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateComputeShader", stub::clear_outputs(ppComputeShader));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateClassLinkage", stub::clear_outputs(ppLinkage));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateBlendState(const D3D11_BLEND_DESC*,
                                                        ID3D11BlendState** ppBlendState) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateBlendState", stub::clear_outputs(ppBlendState));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC*, ID3D11DepthStencilState** ppDepthStencilState) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateDepthStencilState",
                   stub::clear_outputs(ppDepthStencilState));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateRasterizerState(
    const D3D11_RASTERIZER_DESC*, ID3D11RasterizerState** ppRasterizerState) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateRasterizerState", stub::clear_outputs(ppRasterizerState));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateSamplerState(
    const D3D11_SAMPLER_DESC*, ID3D11SamplerState** ppSamplerState) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateSamplerState", stub::clear_outputs(ppSamplerState));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateQuery(const D3D11_QUERY_DESC*,
                                                   ID3D11Query** ppQuery) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateQuery", stub::clear_outputs(ppQuery));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreatePredicate(const D3D11_QUERY_DESC*,
                                                       ID3D11Predicate** ppPredicate) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreatePredicate", stub::clear_outputs(ppPredicate));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CreateCounter(const D3D11_COUNTER_DESC*,
                                                     ID3D11Counter** ppCounter) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateCounter", stub::clear_outputs(ppCounter));
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CreateDeferredContext(UINT, ID3D11DeviceContext** ppDeferredContext) noexcept {
    DM_STUB_RETURN("ID3D11Device::CreateDeferredContext", stub::clear_outputs(ppDeferredContext));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::OpenSharedResource(HANDLE, REFIID,
                                                          void** ppResource) noexcept {
    DM_STUB_RETURN("ID3D11Device::OpenSharedResource", stub::clear_outputs(ppResource));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckFormatSupport(DXGI_FORMAT,
                                                          UINT* pFormatSupport) noexcept {
    DM_STUB_RETURN("ID3D11Device::CheckFormatSupport", stub::clear_outputs(pFormatSupport));
}

HRESULT STDMETHODCALLTYPE
DeviceStubs::CheckMultisampleQualityLevels(DXGI_FORMAT, UINT, UINT* pNumQualityLevels) noexcept {
    DM_STUB_RETURN("ID3D11Device::CheckMultisampleQualityLevels",
                   stub::clear_outputs(pNumQualityLevels));
}

void STDMETHODCALLTYPE DeviceStubs::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) noexcept {
    DM_STUB_RETURN("ID3D11Device::CheckCounterInfo", (void)stub::clear_outputs(pCounterInfo));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckCounter(const D3D11_COUNTER_DESC*,
                                                    D3D11_COUNTER_TYPE* pType,
                                                    UINT* pActiveCounters, LPSTR, UINT* pNameLength,
                                                    LPSTR, UINT* pUnitsLength, LPSTR,
                                                    UINT* pDescriptionLength) noexcept {
    DM_STUB_RETURN(
        "ID3D11Device::CheckCounter",
        stub::clear_outputs(pType, pActiveCounters, pNameLength, pUnitsLength, pDescriptionLength));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::CheckFeatureSupport(D3D11_FEATURE, void* pFeatureSupportData,
                                                           UINT FeatureSupportDataSize) noexcept {
    DM_STUB_RETURN("ID3D11Device::CheckFeatureSupport",
                   stub::clear_bytes(pFeatureSupportData, FeatureSupportDataSize));
}

HRESULT STDMETHODCALLTYPE DeviceStubs::SetExceptionMode(UINT) noexcept {
    DM_STUB("ID3D11Device::SetExceptionMode");
}

UINT STDMETHODCALLTYPE DeviceStubs::GetExceptionMode() noexcept {
    DM_STUB_RETURN("ID3D11Device::GetExceptionMode", 0);
}

} // namespace directmetal::d3d11
