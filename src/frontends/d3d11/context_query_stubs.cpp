#include "context_query_stubs.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::d3d11 {

void STDMETHODCALLTYPE
ContextQueryStubs::PSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSGetShaderResources");
}

void STDMETHODCALLTYPE ContextQueryStubs::PSGetShader(ID3D11PixelShader**, ID3D11ClassInstance**,
                                                      UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSGetShader");
}

void STDMETHODCALLTYPE ContextQueryStubs::PSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSGetSamplers");
}

void STDMETHODCALLTYPE ContextQueryStubs::VSGetShader(ID3D11VertexShader**, ID3D11ClassInstance**,
                                                      UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSGetShader");
}

void STDMETHODCALLTYPE ContextQueryStubs::PSGetConstantBuffers(UINT, UINT,
                                                               ID3D11Buffer**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::PSGetConstantBuffers");
}

void STDMETHODCALLTYPE ContextQueryStubs::IAGetInputLayout(ID3D11InputLayout**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IAGetInputLayout");
}

void STDMETHODCALLTYPE ContextQueryStubs::IAGetVertexBuffers(UINT, UINT, ID3D11Buffer**, UINT*,
                                                             UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IAGetVertexBuffers");
}

void STDMETHODCALLTYPE ContextQueryStubs::IAGetIndexBuffer(ID3D11Buffer**, DXGI_FORMAT*,
                                                           UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IAGetIndexBuffer");
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetConstantBuffers(UINT, UINT,
                                                               ID3D11Buffer**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSGetConstantBuffers");
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetShader(ID3D11GeometryShader**, ID3D11ClassInstance**,
                                                      UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSGetShader");
}

void STDMETHODCALLTYPE
ContextQueryStubs::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::IAGetPrimitiveTopology");
}

void STDMETHODCALLTYPE
ContextQueryStubs::VSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSGetShaderResources");
}

void STDMETHODCALLTYPE ContextQueryStubs::VSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::VSGetSamplers");
}

void STDMETHODCALLTYPE ContextQueryStubs::GetPredication(ID3D11Predicate**, WINBOOL*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GetPredication");
}

void STDMETHODCALLTYPE
ContextQueryStubs::GSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSGetShaderResources");
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GSGetSamplers");
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetRenderTargets(UINT, ID3D11RenderTargetView**,
                                                             ID3D11DepthStencilView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMGetRenderTargets");
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetRenderTargetsAndUnorderedAccessViews(
    UINT, ID3D11RenderTargetView**, ID3D11DepthStencilView**, UINT, UINT,
    ID3D11UnorderedAccessView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews");
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetBlendState(ID3D11BlendState**, FLOAT[4],
                                                          UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMGetBlendState");
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetDepthStencilState(ID3D11DepthStencilState**,
                                                                 UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::OMGetDepthStencilState");
}

void STDMETHODCALLTYPE ContextQueryStubs::SOGetTargets(UINT, ID3D11Buffer**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::SOGetTargets");
}

void STDMETHODCALLTYPE ContextQueryStubs::RSGetState(ID3D11RasterizerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::RSGetState");
}

void STDMETHODCALLTYPE ContextQueryStubs::RSGetViewports(UINT*, D3D11_VIEWPORT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::RSGetViewports");
}

void STDMETHODCALLTYPE ContextQueryStubs::RSGetScissorRects(UINT*, D3D11_RECT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::RSGetScissorRects");
}

void STDMETHODCALLTYPE
ContextQueryStubs::HSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSGetShaderResources");
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetShader(ID3D11HullShader**, ID3D11ClassInstance**,
                                                      UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSGetShader");
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSGetSamplers");
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetConstantBuffers(UINT, UINT,
                                                               ID3D11Buffer**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSGetConstantBuffers");
}

void STDMETHODCALLTYPE
ContextQueryStubs::DSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSGetShaderResources");
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetShader(ID3D11DomainShader**, ID3D11ClassInstance**,
                                                      UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSGetShader");
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSGetSamplers");
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetConstantBuffers(UINT, UINT,
                                                               ID3D11Buffer**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSGetConstantBuffers");
}

void STDMETHODCALLTYPE
ContextQueryStubs::CSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSGetShaderResources");
}

void STDMETHODCALLTYPE
ContextQueryStubs::CSGetUnorderedAccessViews(UINT, UINT, ID3D11UnorderedAccessView**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSGetUnorderedAccessViews");
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetShader(ID3D11ComputeShader**, ID3D11ClassInstance**,
                                                      UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSGetShader");
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetSamplers(UINT, UINT, ID3D11SamplerState**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSGetSamplers");
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetConstantBuffers(UINT, UINT,
                                                               ID3D11Buffer**) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSGetConstantBuffers");
}

void STDMETHODCALLTYPE ContextQueryStubs::ClearState() noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::ClearState");
}

void STDMETHODCALLTYPE ContextQueryStubs::Flush() noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::Flush");
}

UINT STDMETHODCALLTYPE ContextQueryStubs::GetContextFlags() noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GetContextFlags", 0);
}

HRESULT STDMETHODCALLTYPE ContextQueryStubs::FinishCommandList(WINBOOL,
                                                               ID3D11CommandList**) noexcept {
    DM_STUB("ID3D11DeviceContext::FinishCommandList");
}

} // namespace directmetal::d3d11
