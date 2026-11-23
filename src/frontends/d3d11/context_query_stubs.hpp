#pragma once

#include "context_transfer_stubs.hpp"

namespace directmetal::d3d11 {

// ID3D11DeviceContext methods outside the implemented slice, part 3 of 3 in vtable order.
// Each returns E_NOTIMPL or does nothing.
class ContextQueryStubs : public ContextTransferStubs {
public:
    void STDMETHODCALLTYPE PSGetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView**) noexcept override;
    void STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader**, ID3D11ClassInstance**,
                                       UINT*) noexcept override;
    void STDMETHODCALLTYPE PSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept override;
    void STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader**, ID3D11ClassInstance**,
                                       UINT*) noexcept override;
    void STDMETHODCALLTYPE PSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) noexcept override;
    void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout**) noexcept override;
    void STDMETHODCALLTYPE IAGetVertexBuffers(UINT, UINT, ID3D11Buffer**, UINT*,
                                              UINT*) noexcept override;
    void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11Buffer**, DXGI_FORMAT*, UINT*) noexcept override;
    void STDMETHODCALLTYPE GSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) noexcept override;
    void STDMETHODCALLTYPE GSGetShader(ID3D11GeometryShader**, ID3D11ClassInstance**,
                                       UINT*) noexcept override;
    void STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY*) noexcept override;
    void STDMETHODCALLTYPE VSGetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView**) noexcept override;
    void STDMETHODCALLTYPE VSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept override;
    void STDMETHODCALLTYPE GetPredication(ID3D11Predicate**, WINBOOL*) noexcept override;
    void STDMETHODCALLTYPE GSGetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView**) noexcept override;
    void STDMETHODCALLTYPE GSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept override;
    void STDMETHODCALLTYPE OMGetRenderTargets(UINT, ID3D11RenderTargetView**,
                                              ID3D11DepthStencilView**) noexcept override;
    void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
        UINT, ID3D11RenderTargetView**, ID3D11DepthStencilView**, UINT, UINT,
        ID3D11UnorderedAccessView**) noexcept override;
    void STDMETHODCALLTYPE OMGetBlendState(ID3D11BlendState**, FLOAT[4], UINT*) noexcept override;
    void STDMETHODCALLTYPE OMGetDepthStencilState(ID3D11DepthStencilState**,
                                                  UINT*) noexcept override;
    void STDMETHODCALLTYPE SOGetTargets(UINT, ID3D11Buffer**) noexcept override;
    void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState**) noexcept override;
    void STDMETHODCALLTYPE RSGetViewports(UINT*, D3D11_VIEWPORT*) noexcept override;
    void STDMETHODCALLTYPE RSGetScissorRects(UINT*, D3D11_RECT*) noexcept override;
    void STDMETHODCALLTYPE HSGetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView**) noexcept override;
    void STDMETHODCALLTYPE HSGetShader(ID3D11HullShader**, ID3D11ClassInstance**,
                                       UINT*) noexcept override;
    void STDMETHODCALLTYPE HSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept override;
    void STDMETHODCALLTYPE HSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) noexcept override;
    void STDMETHODCALLTYPE DSGetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView**) noexcept override;
    void STDMETHODCALLTYPE DSGetShader(ID3D11DomainShader**, ID3D11ClassInstance**,
                                       UINT*) noexcept override;
    void STDMETHODCALLTYPE DSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept override;
    void STDMETHODCALLTYPE DSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) noexcept override;
    void STDMETHODCALLTYPE CSGetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView**) noexcept override;
    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(UINT, UINT,
                                                     ID3D11UnorderedAccessView**) noexcept override;
    void STDMETHODCALLTYPE CSGetShader(ID3D11ComputeShader**, ID3D11ClassInstance**,
                                       UINT*) noexcept override;
    void STDMETHODCALLTYPE CSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept override;
    void STDMETHODCALLTYPE CSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) noexcept override;
    void STDMETHODCALLTYPE ClearState() noexcept override;
    void STDMETHODCALLTYPE Flush() noexcept override;
    UINT STDMETHODCALLTYPE GetContextFlags() noexcept override;
    HRESULT STDMETHODCALLTYPE FinishCommandList(WINBOOL, ID3D11CommandList**) noexcept override;
};

} // namespace directmetal::d3d11
