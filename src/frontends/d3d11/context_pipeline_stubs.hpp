#pragma once

#include <d3d11.h>

namespace directmetal::d3d11 {

// ID3D11DeviceContext methods outside the implemented slice, part 1 of 3 in vtable order.
// Each returns E_NOTIMPL or does nothing.
class ContextPipelineStubs : public ID3D11DeviceContext {
public:
    void STDMETHODCALLTYPE VSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) noexcept override;
    void STDMETHODCALLTYPE PSSetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView* const*) noexcept override;
    void STDMETHODCALLTYPE PSSetShader(ID3D11PixelShader*, ID3D11ClassInstance* const*,
                                       UINT) noexcept override;
    void STDMETHODCALLTYPE PSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) noexcept override;
    void STDMETHODCALLTYPE VSSetShader(ID3D11VertexShader*, ID3D11ClassInstance* const*,
                                       UINT) noexcept override;
    void STDMETHODCALLTYPE DrawIndexed(UINT, UINT, INT) noexcept override;
    void STDMETHODCALLTYPE Draw(UINT, UINT) noexcept override;
    void STDMETHODCALLTYPE PSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) noexcept override;
    void STDMETHODCALLTYPE IASetInputLayout(ID3D11InputLayout*) noexcept override;
    void STDMETHODCALLTYPE IASetVertexBuffers(UINT, UINT, ID3D11Buffer* const*, const UINT*,
                                              const UINT*) noexcept override;
    void STDMETHODCALLTYPE IASetIndexBuffer(ID3D11Buffer*, DXGI_FORMAT, UINT) noexcept override;
    void STDMETHODCALLTYPE DrawIndexedInstanced(UINT, UINT, UINT, INT, UINT) noexcept override;
    void STDMETHODCALLTYPE DrawInstanced(UINT, UINT, UINT, UINT) noexcept override;
    void STDMETHODCALLTYPE GSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) noexcept override;
    void STDMETHODCALLTYPE GSSetShader(ID3D11GeometryShader*, ID3D11ClassInstance* const*,
                                       UINT) noexcept override;
    void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY) noexcept override;
    void STDMETHODCALLTYPE VSSetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView* const*) noexcept override;
    void STDMETHODCALLTYPE VSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) noexcept override;
    void STDMETHODCALLTYPE Begin(ID3D11Asynchronous*) noexcept override;
    void STDMETHODCALLTYPE End(ID3D11Asynchronous*) noexcept override;
    HRESULT STDMETHODCALLTYPE GetData(ID3D11Asynchronous*, void*, UINT, UINT) noexcept override;
    void STDMETHODCALLTYPE SetPredication(ID3D11Predicate*, WINBOOL) noexcept override;
    void STDMETHODCALLTYPE GSSetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView* const*) noexcept override;
    void STDMETHODCALLTYPE GSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) noexcept override;
    void STDMETHODCALLTYPE OMSetRenderTargets(UINT, ID3D11RenderTargetView* const*,
                                              ID3D11DepthStencilView*) noexcept override;
    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
        UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*, UINT, UINT,
        ID3D11UnorderedAccessView* const*, const UINT*) noexcept override;
    void STDMETHODCALLTYPE OMSetBlendState(ID3D11BlendState*, const FLOAT[4],
                                           UINT) noexcept override;
    void STDMETHODCALLTYPE OMSetDepthStencilState(ID3D11DepthStencilState*, UINT) noexcept override;
    void STDMETHODCALLTYPE SOSetTargets(UINT, ID3D11Buffer* const*, const UINT*) noexcept override;
    void STDMETHODCALLTYPE DrawAuto() noexcept override;
    void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(ID3D11Buffer*, UINT) noexcept override;
    void STDMETHODCALLTYPE DrawInstancedIndirect(ID3D11Buffer*, UINT) noexcept override;
    void STDMETHODCALLTYPE Dispatch(UINT, UINT, UINT) noexcept override;
    void STDMETHODCALLTYPE DispatchIndirect(ID3D11Buffer*, UINT) noexcept override;
    void STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState*) noexcept override;
    void STDMETHODCALLTYPE RSSetViewports(UINT, const D3D11_VIEWPORT*) noexcept override;
    void STDMETHODCALLTYPE RSSetScissorRects(UINT, const D3D11_RECT*) noexcept override;
    void STDMETHODCALLTYPE CopySubresourceRegion(ID3D11Resource*, UINT, UINT, UINT, UINT,
                                                 ID3D11Resource*, UINT,
                                                 const D3D11_BOX*) noexcept override;
};

} // namespace directmetal::d3d11
