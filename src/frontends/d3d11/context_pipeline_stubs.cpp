#include "context_pipeline_stubs.hpp"

#include "stub_outputs.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::d3d11 {

void STDMETHODCALLTYPE ContextPipelineStubs::VSSetConstantBuffers(UINT, UINT,
                                                                  ID3D11Buffer* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSSetConstantBuffers");
}

void STDMETHODCALLTYPE
ContextPipelineStubs::PSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSSetShaderResources");
}

void STDMETHODCALLTYPE ContextPipelineStubs::PSSetShader(ID3D11PixelShader*,
                                                         ID3D11ClassInstance* const*,
                                                         UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSSetShader");
}

void STDMETHODCALLTYPE ContextPipelineStubs::PSSetSamplers(UINT, UINT,
                                                           ID3D11SamplerState* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSSetSamplers");
}

void STDMETHODCALLTYPE ContextPipelineStubs::VSSetShader(ID3D11VertexShader*,
                                                         ID3D11ClassInstance* const*,
                                                         UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSSetShader");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DrawIndexed(UINT, UINT, INT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DrawIndexed");
}

void STDMETHODCALLTYPE ContextPipelineStubs::Draw(UINT, UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::Draw");
}

void STDMETHODCALLTYPE ContextPipelineStubs::PSSetConstantBuffers(UINT, UINT,
                                                                  ID3D11Buffer* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSSetConstantBuffers");
}

void STDMETHODCALLTYPE ContextPipelineStubs::IASetInputLayout(ID3D11InputLayout*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IASetInputLayout");
}

void STDMETHODCALLTYPE ContextPipelineStubs::IASetVertexBuffers(UINT, UINT, ID3D11Buffer* const*,
                                                                const UINT*, const UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IASetVertexBuffers");
}

void STDMETHODCALLTYPE ContextPipelineStubs::IASetIndexBuffer(ID3D11Buffer*, DXGI_FORMAT,
                                                              UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IASetIndexBuffer");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DrawIndexedInstanced(UINT, UINT, UINT, INT,
                                                                  UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DrawIndexedInstanced");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DrawInstanced(UINT, UINT, UINT, UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DrawInstanced");
}

void STDMETHODCALLTYPE ContextPipelineStubs::GSSetConstantBuffers(UINT, UINT,
                                                                  ID3D11Buffer* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSSetConstantBuffers");
}

void STDMETHODCALLTYPE ContextPipelineStubs::GSSetShader(ID3D11GeometryShader*,
                                                         ID3D11ClassInstance* const*,
                                                         UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSSetShader");
}

void STDMETHODCALLTYPE
ContextPipelineStubs::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IASetPrimitiveTopology");
}

void STDMETHODCALLTYPE
ContextPipelineStubs::VSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSSetShaderResources");
}

void STDMETHODCALLTYPE ContextPipelineStubs::VSSetSamplers(UINT, UINT,
                                                           ID3D11SamplerState* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSSetSamplers");
}

void STDMETHODCALLTYPE ContextPipelineStubs::Begin(ID3D11Asynchronous*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::Begin");
}

void STDMETHODCALLTYPE ContextPipelineStubs::End(ID3D11Asynchronous*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::End");
}

HRESULT STDMETHODCALLTYPE ContextPipelineStubs::GetData(ID3D11Asynchronous*, void* pData,
                                                        UINT DataSize, UINT) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GetData", stub::clear_bytes(pData, DataSize));
}

void STDMETHODCALLTYPE ContextPipelineStubs::SetPredication(ID3D11Predicate*, WINBOOL) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::SetPredication");
}

void STDMETHODCALLTYPE
ContextPipelineStubs::GSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSSetShaderResources");
}

void STDMETHODCALLTYPE ContextPipelineStubs::GSSetSamplers(UINT, UINT,
                                                           ID3D11SamplerState* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSSetSamplers");
}

void STDMETHODCALLTYPE ContextPipelineStubs::OMSetRenderTargets(UINT,
                                                                ID3D11RenderTargetView* const*,
                                                                ID3D11DepthStencilView*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMSetRenderTargets");
}

void STDMETHODCALLTYPE ContextPipelineStubs::OMSetRenderTargetsAndUnorderedAccessViews(
    UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*, UINT, UINT,
    ID3D11UnorderedAccessView* const*, const UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews");
}

void STDMETHODCALLTYPE ContextPipelineStubs::OMSetBlendState(ID3D11BlendState*, const FLOAT[4],
                                                             UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMSetBlendState");
}

void STDMETHODCALLTYPE ContextPipelineStubs::OMSetDepthStencilState(ID3D11DepthStencilState*,
                                                                    UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMSetDepthStencilState");
}

void STDMETHODCALLTYPE ContextPipelineStubs::SOSetTargets(UINT, ID3D11Buffer* const*,
                                                          const UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::SOSetTargets");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DrawAuto() noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DrawAuto");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DrawIndexedInstancedIndirect(ID3D11Buffer*,
                                                                          UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DrawIndexedInstancedIndirect");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DrawInstancedIndirect(ID3D11Buffer*, UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DrawInstancedIndirect");
}

void STDMETHODCALLTYPE ContextPipelineStubs::Dispatch(UINT, UINT, UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::Dispatch");
}

void STDMETHODCALLTYPE ContextPipelineStubs::DispatchIndirect(ID3D11Buffer*, UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DispatchIndirect");
}

void STDMETHODCALLTYPE ContextPipelineStubs::RSSetState(ID3D11RasterizerState*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::RSSetState");
}

void STDMETHODCALLTYPE ContextPipelineStubs::RSSetViewports(UINT, const D3D11_VIEWPORT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::RSSetViewports");
}

void STDMETHODCALLTYPE ContextPipelineStubs::RSSetScissorRects(UINT, const D3D11_RECT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::RSSetScissorRects");
}

void STDMETHODCALLTYPE ContextPipelineStubs::CopySubresourceRegion(ID3D11Resource*, UINT, UINT,
                                                                   UINT, UINT, ID3D11Resource*,
                                                                   UINT,
                                                                   const D3D11_BOX*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CopySubresourceRegion");
}

} // namespace directmetal::d3d11
