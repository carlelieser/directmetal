#include "context_transfer_stubs.hpp"

#include "stub_outputs.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::d3d11 {

void STDMETHODCALLTYPE ContextTransferStubs::UpdateSubresource(ID3D11Resource*, UINT,
                                                               const D3D11_BOX*, const void*, UINT,
                                                               UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::UpdateSubresource");
}

void STDMETHODCALLTYPE
ContextTransferStubs::CopyStructureCount(ID3D11Buffer*, UINT, ID3D11UnorderedAccessView*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CopyStructureCount");
}

void STDMETHODCALLTYPE ContextTransferStubs::ClearUnorderedAccessViewUint(
    ID3D11UnorderedAccessView*, const UINT[4]) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::ClearUnorderedAccessViewUint");
}

void STDMETHODCALLTYPE ContextTransferStubs::ClearUnorderedAccessViewFloat(
    ID3D11UnorderedAccessView*, const FLOAT[4]) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::ClearUnorderedAccessViewFloat");
}

void STDMETHODCALLTYPE ContextTransferStubs::ClearDepthStencilView(ID3D11DepthStencilView*, UINT,
                                                                   FLOAT, UINT8) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::ClearDepthStencilView");
}

void STDMETHODCALLTYPE ContextTransferStubs::GenerateMips(ID3D11ShaderResourceView*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::GenerateMips");
}

void STDMETHODCALLTYPE ContextTransferStubs::SetResourceMinLOD(ID3D11Resource*, FLOAT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::SetResourceMinLOD");
}

FLOAT STDMETHODCALLTYPE ContextTransferStubs::GetResourceMinLOD(ID3D11Resource*) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::GetResourceMinLOD", 0.0f);
}

void STDMETHODCALLTYPE ContextTransferStubs::ResolveSubresource(ID3D11Resource*, UINT,
                                                                ID3D11Resource*, UINT,
                                                                DXGI_FORMAT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::ResolveSubresource");
}

void STDMETHODCALLTYPE ContextTransferStubs::ExecuteCommandList(ID3D11CommandList*,
                                                                WINBOOL) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::ExecuteCommandList");
}

void STDMETHODCALLTYPE
ContextTransferStubs::HSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSSetShaderResources");
}

void STDMETHODCALLTYPE ContextTransferStubs::HSSetShader(ID3D11HullShader*,
                                                         ID3D11ClassInstance* const*,
                                                         UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSSetShader");
}

void STDMETHODCALLTYPE ContextTransferStubs::HSSetSamplers(UINT, UINT,
                                                           ID3D11SamplerState* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSSetSamplers");
}

void STDMETHODCALLTYPE ContextTransferStubs::HSSetConstantBuffers(UINT, UINT,
                                                                  ID3D11Buffer* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::HSSetConstantBuffers");
}

void STDMETHODCALLTYPE
ContextTransferStubs::DSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSSetShaderResources");
}

void STDMETHODCALLTYPE ContextTransferStubs::DSSetShader(ID3D11DomainShader*,
                                                         ID3D11ClassInstance* const*,
                                                         UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSSetShader");
}

void STDMETHODCALLTYPE ContextTransferStubs::DSSetSamplers(UINT, UINT,
                                                           ID3D11SamplerState* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSSetSamplers");
}

void STDMETHODCALLTYPE ContextTransferStubs::DSSetConstantBuffers(UINT, UINT,
                                                                  ID3D11Buffer* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::DSSetConstantBuffers");
}

void STDMETHODCALLTYPE
ContextTransferStubs::CSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSSetShaderResources");
}

void STDMETHODCALLTYPE ContextTransferStubs::CSSetUnorderedAccessViews(
    UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSSetUnorderedAccessViews");
}

void STDMETHODCALLTYPE ContextTransferStubs::CSSetShader(ID3D11ComputeShader*,
                                                         ID3D11ClassInstance* const*,
                                                         UINT) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSSetShader");
}

void STDMETHODCALLTYPE ContextTransferStubs::CSSetSamplers(UINT, UINT,
                                                           ID3D11SamplerState* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSSetSamplers");
}

void STDMETHODCALLTYPE ContextTransferStubs::CSSetConstantBuffers(UINT, UINT,
                                                                  ID3D11Buffer* const*) noexcept {
    DM_STUB_VOID("ID3D11DeviceContext::CSSetConstantBuffers");
}

void STDMETHODCALLTYPE ContextTransferStubs::VSGetConstantBuffers(
    UINT, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) noexcept {
    DM_STUB_RETURN("ID3D11DeviceContext::VSGetConstantBuffers",
                   (void)stub::clear_array(NumBuffers, ppConstantBuffers));
}

} // namespace directmetal::d3d11
