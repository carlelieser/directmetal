#pragma once

#include <d3d11.h>

namespace directmetal::d3d11 {

// Every ID3D11Device method outside the implemented slice. Each returns E_NOTIMPL or does nothing.
class DeviceStubs : public ID3D11Device {
public:
    HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*,
                                           ID3D11Buffer**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC*,
                                              const D3D11_SUBRESOURCE_DATA*,
                                              ID3D11Texture1D**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC*,
                                              const D3D11_SUBRESOURCE_DATA*,
                                              ID3D11Texture3D**) noexcept override;
    HRESULT STDMETHODCALLTYPE
    CreateShaderResourceView(ID3D11Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC*,
                             ID3D11ShaderResourceView**) noexcept override;
    HRESULT STDMETHODCALLTYPE
    CreateUnorderedAccessView(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC*,
                              ID3D11UnorderedAccessView**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource*,
                                                     const D3D11_DEPTH_STENCIL_VIEW_DESC*,
                                                     ID3D11DepthStencilView**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC*, UINT, const void*,
                                                SIZE_T, ID3D11InputLayout**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                 ID3D11VertexShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                   ID3D11GeometryShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
        const void*, SIZE_T, const D3D11_SO_DECLARATION_ENTRY*, UINT, const UINT*, UINT, UINT,
        ID3D11ClassLinkage*, ID3D11GeometryShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                ID3D11PixelShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateHullShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                               ID3D11HullShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateDomainShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                 ID3D11DomainShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateComputeShader(const void*, SIZE_T, ID3D11ClassLinkage*,
                                                  ID3D11ComputeShader**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC*,
                                               ID3D11BlendState**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC*,
                                                      ID3D11DepthStencilState**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC*,
                                                    ID3D11RasterizerState**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC*,
                                                 ID3D11SamplerState**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC*, ID3D11Query**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC*,
                                              ID3D11Predicate**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC*,
                                            ID3D11Counter**) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT, ID3D11DeviceContext**) noexcept override;
    HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE, REFIID, void**) noexcept override;
    HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT, UINT*) noexcept override;
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT, UINT,
                                                            UINT*) noexcept override;
    void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO*) noexcept override;
    HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC*, D3D11_COUNTER_TYPE*, UINT*,
                                           LPSTR, UINT*, LPSTR, UINT*, LPSTR,
                                           UINT*) noexcept override;
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE, void*, UINT) noexcept override;
    D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() noexcept override;
    UINT STDMETHODCALLTYPE GetCreationFlags() noexcept override;
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() noexcept override;
    HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT) noexcept override;
    UINT STDMETHODCALLTYPE GetExceptionMode() noexcept override;
};

} // namespace directmetal::d3d11
