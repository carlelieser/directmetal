#include "d3d11_fixture.hpp"

#include <array>
#include <cstdint>

namespace {

using StubOutputsTest = D3d11Test;

template <typename Interface> Interface* sentinel() {
    static int storage = 0;
    return reinterpret_cast<Interface*>(&storage);
}

TEST_F(StubOutputsTest, CreationStubsNullTheirOutput) {
    const D3D11_BUFFER_DESC desc{};
    ID3D11Buffer* buffer = sentinel<ID3D11Buffer>();
    EXPECT_EQ(device_->CreateBuffer(&desc, nullptr, &buffer), E_NOTIMPL);
    EXPECT_EQ(buffer, nullptr);
    ID3D11DeviceContext* deferred = sentinel<ID3D11DeviceContext>();
    EXPECT_EQ(device_->CreateDeferredContext(0, &deferred), E_NOTIMPL);
    EXPECT_EQ(deferred, nullptr);
    void* shared = &deferred;
    EXPECT_EQ(device_->OpenSharedResource(nullptr, __uuidof(ID3D11Texture2D), &shared), E_NOTIMPL);
    EXPECT_EQ(shared, nullptr);
}

TEST_F(StubOutputsTest, QueryStubsZeroTheirOutputs) {
    UINT support = 0xffffffff;
    EXPECT_EQ(device_->CheckFormatSupport(DXGI_FORMAT_R8G8B8A8_UNORM, &support), E_NOTIMPL);
    EXPECT_EQ(support, 0u);
    std::array<std::uint8_t, 8> data;
    data.fill(0xff);
    EXPECT_EQ(device_->CheckFeatureSupport(D3D11_FEATURE_THREADING, data.data(),
                                           static_cast<UINT>(data.size())),
              E_NOTIMPL);
    EXPECT_EQ(data, (std::array<std::uint8_t, 8>{}));
}

TEST_F(StubOutputsTest, StateGettersClearEveryOutput) {
    ID3D11VertexShader* shader = sentinel<ID3D11VertexShader>();
    UINT instances = 5;
    context_->VSGetShader(&shader, nullptr, &instances);
    EXPECT_EQ(shader, nullptr);
    EXPECT_EQ(instances, 0u);
    std::array<ID3D11Buffer*, 2> buffers = {sentinel<ID3D11Buffer>(), sentinel<ID3D11Buffer>()};
    std::array<UINT, 2> strides = {7, 7};
    std::array<UINT, 2> offsets = {9, 9};
    context_->IAGetVertexBuffers(0, 2, buffers.data(), strides.data(), offsets.data());
    EXPECT_EQ(buffers, (std::array<ID3D11Buffer*, 2>{}));
    EXPECT_EQ(strides, (std::array<UINT, 2>{}));
    EXPECT_EQ(offsets, (std::array<UINT, 2>{}));
    std::array<FLOAT, 4> factors = {1, 1, 1, 1};
    UINT mask = 3;
    ID3D11BlendState* blend = sentinel<ID3D11BlendState>();
    context_->OMGetBlendState(&blend, factors.data(), &mask);
    EXPECT_EQ(blend, nullptr);
    EXPECT_EQ(factors, (std::array<FLOAT, 4>{}));
    EXPECT_EQ(mask, 0u);
    UINT viewports = 4;
    context_->RSGetViewports(&viewports, nullptr);
    EXPECT_EQ(viewports, 0u);
    ID3D11RenderTargetView* target = sentinel<ID3D11RenderTargetView>();
    ID3D11DepthStencilView* depth = sentinel<ID3D11DepthStencilView>();
    context_->OMGetRenderTargets(1, &target, &depth);
    EXPECT_EQ(target, nullptr);
    EXPECT_EQ(depth, nullptr);
}

} // namespace
