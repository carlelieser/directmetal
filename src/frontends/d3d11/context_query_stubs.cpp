#include "context_query_stubs.hpp"

#include "stub_outputs.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::d3d11 {

void STDMETHODCALLTYPE ContextQueryStubs::PSGetShaderResources(
    UINT, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::PSGetShaderResources",
                   (void)stub::clear_array(NumViews, ppShaderResourceViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::PSGetShader(ID3D11PixelShader** ppPixelShader,
                                                      ID3D11ClassInstance**,
                                                      UINT* pNumClassInstances) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::PSGetShader",
                   (void)stub::clear_outputs(ppPixelShader, pNumClassInstances));
}

void STDMETHODCALLTYPE ContextQueryStubs::PSGetSamplers(UINT, UINT NumSamplers,
                                                        ID3D11SamplerState** ppSamplers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::PSGetSamplers",
                   (void)stub::clear_array(NumSamplers, ppSamplers));
}

void STDMETHODCALLTYPE ContextQueryStubs::VSGetShader(ID3D11VertexShader** ppVertexShader,
                                                      ID3D11ClassInstance**,
                                                      UINT* pNumClassInstances) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::VSGetShader",
                   (void)stub::clear_outputs(ppVertexShader, pNumClassInstances));
}

void STDMETHODCALLTYPE ContextQueryStubs::PSGetConstantBuffers(
    UINT, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::PSGetConstantBuffers",
                   (void)stub::clear_array(NumBuffers, ppConstantBuffers));
}

void STDMETHODCALLTYPE
ContextQueryStubs::IAGetInputLayout(ID3D11InputLayout** ppInputLayout) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::IAGetInputLayout",
                   (void)stub::clear_outputs(ppInputLayout));
}

void STDMETHODCALLTYPE ContextQueryStubs::IAGetVertexBuffers(UINT, UINT NumBuffers,
                                                             ID3D11Buffer** ppVertexBuffers,
                                                             UINT* pStrides,
                                                             UINT* pOffsets) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::IAGetVertexBuffers",
                   (void)stub::all(stub::clear_array(NumBuffers, ppVertexBuffers),
                                   stub::clear_array(NumBuffers, pStrides),
                                   stub::clear_array(NumBuffers, pOffsets)));
}

void STDMETHODCALLTYPE ContextQueryStubs::IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer,
                                                           DXGI_FORMAT* Format,
                                                           UINT* Offset) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::IAGetIndexBuffer",
                   (void)stub::clear_outputs(pIndexBuffer, Format, Offset));
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetConstantBuffers(
    UINT, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GSGetConstantBuffers",
                   (void)stub::clear_array(NumBuffers, ppConstantBuffers));
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetShader(ID3D11GeometryShader** ppGeometryShader,
                                                      ID3D11ClassInstance**,
                                                      UINT* pNumClassInstances) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GSGetShader",
                   (void)stub::clear_outputs(ppGeometryShader, pNumClassInstances));
}

void STDMETHODCALLTYPE
ContextQueryStubs::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::IAGetPrimitiveTopology",
                   (void)stub::clear_outputs(pTopology));
}

void STDMETHODCALLTYPE ContextQueryStubs::VSGetShaderResources(
    UINT, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::VSGetShaderResources",
                   (void)stub::clear_array(NumViews, ppShaderResourceViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::VSGetSamplers(UINT, UINT NumSamplers,
                                                        ID3D11SamplerState** ppSamplers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::VSGetSamplers",
                   (void)stub::clear_array(NumSamplers, ppSamplers));
}

void STDMETHODCALLTYPE ContextQueryStubs::GetPredication(ID3D11Predicate** ppPredicate,
                                                         WINBOOL* pPredicateValue) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GetPredication",
                   (void)stub::clear_outputs(ppPredicate, pPredicateValue));
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetShaderResources(
    UINT, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GSGetShaderResources",
                   (void)stub::clear_array(NumViews, ppShaderResourceViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::GSGetSamplers(UINT, UINT NumSamplers,
                                                        ID3D11SamplerState** ppSamplers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GSGetSamplers",
                   (void)stub::clear_array(NumSamplers, ppSamplers));
}

void STDMETHODCALLTYPE
ContextQueryStubs::OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews,
                                      ID3D11DepthStencilView** ppDepthStencilView) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::OMGetRenderTargets",
                   (void)stub::all(stub::clear_array(NumViews, ppRenderTargetViews),
                                   stub::clear_outputs(ppDepthStencilView)));
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetRenderTargetsAndUnorderedAccessViews(
    UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews,
    ID3D11DepthStencilView** ppDepthStencilView, UINT, UINT NumUAVs,
    ID3D11UnorderedAccessView** ppUnorderedAccessViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews",
                   (void)stub::all(stub::clear_array(NumRTVs, ppRenderTargetViews),
                                   stub::clear_outputs(ppDepthStencilView),
                                   stub::clear_array(NumUAVs, ppUnorderedAccessViews)));
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetBlendState(ID3D11BlendState** ppBlendState,
                                                          FLOAT BlendFactor[4],
                                                          UINT* pSampleMask) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::OMGetBlendState",
                   (void)stub::all(stub::clear_outputs(ppBlendState, pSampleMask),
                                   stub::clear_array(4, BlendFactor)));
}

void STDMETHODCALLTYPE ContextQueryStubs::OMGetDepthStencilState(
    ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::OMGetDepthStencilState",
                   (void)stub::clear_outputs(ppDepthStencilState, pStencilRef));
}

void STDMETHODCALLTYPE ContextQueryStubs::SOGetTargets(UINT NumBuffers,
                                                       ID3D11Buffer** ppSOTargets) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::SOGetTargets",
                   (void)stub::clear_array(NumBuffers, ppSOTargets));
}

void STDMETHODCALLTYPE
ContextQueryStubs::RSGetState(ID3D11RasterizerState** ppRasterizerState) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::RSGetState", (void)stub::clear_outputs(ppRasterizerState));
}

void STDMETHODCALLTYPE ContextQueryStubs::RSGetViewports(UINT* pNumViewports,
                                                         D3D11_VIEWPORT*) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::RSGetViewports", (void)stub::clear_outputs(pNumViewports));
}

void STDMETHODCALLTYPE ContextQueryStubs::RSGetScissorRects(UINT* pNumRects, D3D11_RECT*) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::RSGetScissorRects", (void)stub::clear_outputs(pNumRects));
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetShaderResources(
    UINT, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::HSGetShaderResources",
                   (void)stub::clear_array(NumViews, ppShaderResourceViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetShader(ID3D11HullShader** ppHullShader,
                                                      ID3D11ClassInstance**,
                                                      UINT* pNumClassInstances) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::HSGetShader",
                   (void)stub::clear_outputs(ppHullShader, pNumClassInstances));
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetSamplers(UINT, UINT NumSamplers,
                                                        ID3D11SamplerState** ppSamplers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::HSGetSamplers",
                   (void)stub::clear_array(NumSamplers, ppSamplers));
}

void STDMETHODCALLTYPE ContextQueryStubs::HSGetConstantBuffers(
    UINT, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::HSGetConstantBuffers",
                   (void)stub::clear_array(NumBuffers, ppConstantBuffers));
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetShaderResources(
    UINT, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::DSGetShaderResources",
                   (void)stub::clear_array(NumViews, ppShaderResourceViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetShader(ID3D11DomainShader** ppDomainShader,
                                                      ID3D11ClassInstance**,
                                                      UINT* pNumClassInstances) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::DSGetShader",
                   (void)stub::clear_outputs(ppDomainShader, pNumClassInstances));
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetSamplers(UINT, UINT NumSamplers,
                                                        ID3D11SamplerState** ppSamplers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::DSGetSamplers",
                   (void)stub::clear_array(NumSamplers, ppSamplers));
}

void STDMETHODCALLTYPE ContextQueryStubs::DSGetConstantBuffers(
    UINT, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::DSGetConstantBuffers",
                   (void)stub::clear_array(NumBuffers, ppConstantBuffers));
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetShaderResources(
    UINT, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::CSGetShaderResources",
                   (void)stub::clear_array(NumViews, ppShaderResourceViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetUnorderedAccessViews(
    UINT, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::CSGetUnorderedAccessViews",
                   (void)stub::clear_array(NumUAVs, ppUnorderedAccessViews));
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetShader(ID3D11ComputeShader** ppComputeShader,
                                                      ID3D11ClassInstance**,
                                                      UINT* pNumClassInstances) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::CSGetShader",
                   (void)stub::clear_outputs(ppComputeShader, pNumClassInstances));
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetSamplers(UINT, UINT NumSamplers,
                                                        ID3D11SamplerState** ppSamplers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::CSGetSamplers",
                   (void)stub::clear_array(NumSamplers, ppSamplers));
}

void STDMETHODCALLTYPE ContextQueryStubs::CSGetConstantBuffers(
    UINT, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::CSGetConstantBuffers",
                   (void)stub::clear_array(NumBuffers, ppConstantBuffers));
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

HRESULT STDMETHODCALLTYPE
ContextQueryStubs::FinishCommandList(WINBOOL, ID3D11CommandList** ppCommandList) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::FinishCommandList", stub::clear_outputs(ppCommandList));
}

} // namespace directmetal::d3d11
