#pragma once

#include "context_pipeline_stubs.hpp"

namespace directmetal::d3d11 {

// ID3D11DeviceContext methods outside the implemented slice, part 2 of 3 in vtable order.
// Each returns E_NOTIMPL or does nothing.
class ContextTransferStubs : public ContextPipelineStubs {
public:
    void STDMETHODCALLTYPE UpdateSubresource(ID3D11Resource*, UINT, const D3D11_BOX*, const void*,
                                             UINT, UINT) noexcept override;
    void STDMETHODCALLTYPE CopyStructureCount(ID3D11Buffer*, UINT,
                                              ID3D11UnorderedAccessView*) noexcept override;
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView*,
                                                        const UINT[4]) noexcept override;
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView*,
                                                         const FLOAT[4]) noexcept override;
    void STDMETHODCALLTYPE ClearDepthStencilView(ID3D11DepthStencilView*, UINT, FLOAT,
                                                 UINT8) noexcept override;
    void STDMETHODCALLTYPE GenerateMips(ID3D11ShaderResourceView*) noexcept override;
    void STDMETHODCALLTYPE SetResourceMinLOD(ID3D11Resource*, FLOAT) noexcept override;
    FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource*) noexcept override;
    void STDMETHODCALLTYPE ResolveSubresource(ID3D11Resource*, UINT, ID3D11Resource*, UINT,
                                              DXGI_FORMAT) noexcept override;
    void STDMETHODCALLTYPE ExecuteCommandList(ID3D11CommandList*, WINBOOL) noexcept override;
    void STDMETHODCALLTYPE HSSetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView* const*) noexcept override;
    void STDMETHODCALLTYPE HSSetShader(ID3D11HullShader*, ID3D11ClassInstance* const*,
                                       UINT) noexcept override;
    void STDMETHODCALLTYPE HSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) noexcept override;
    void STDMETHODCALLTYPE HSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) noexcept override;
    void STDMETHODCALLTYPE DSSetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView* const*) noexcept override;
    void STDMETHODCALLTYPE DSSetShader(ID3D11DomainShader*, ID3D11ClassInstance* const*,
                                       UINT) noexcept override;
    void STDMETHODCALLTYPE DSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) noexcept override;
    void STDMETHODCALLTYPE DSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) noexcept override;
    void STDMETHODCALLTYPE CSSetShaderResources(UINT, UINT,
                                                ID3D11ShaderResourceView* const*) noexcept override;
    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(UINT, UINT, ID3D11UnorderedAccessView* const*,
                                                     const UINT*) noexcept override;
    void STDMETHODCALLTYPE CSSetShader(ID3D11ComputeShader*, ID3D11ClassInstance* const*,
                                       UINT) noexcept override;
    void STDMETHODCALLTYPE CSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) noexcept override;
    void STDMETHODCALLTYPE CSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) noexcept override;
    void STDMETHODCALLTYPE VSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) noexcept override;
};

} // namespace directmetal::d3d11
