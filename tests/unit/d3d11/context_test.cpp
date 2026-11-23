#include "d3d11_fixture.hpp"

#include <bit>
#include <cmath>
#include <limits>

namespace {

using directmetal::backend::ClearKind;
using directmetal::bridge::ClearTextureCommand;
using directmetal::bridge::CopyTextureToBufferCommand;

class ContextTest : public D3d11Test {
protected:
    // Clears an 8x8 render target of `format` and returns the recorded command.
    ClearTextureCommand clear(DXGI_FORMAT format, const FLOAT (&color)[4]) {
        ID3D11Texture2D* texture = create_texture(render_target_desc(format));
        ID3D11RenderTargetView* view = nullptr;
        EXPECT_EQ(device_->CreateRenderTargetView(texture, nullptr, &view), S_OK);
        context_->ClearRenderTargetView(view, color);
        view->Release();
        texture->Release();
        flush();
        const auto clears = fake_bridge::commands_of<ClearTextureCommand>();
        EXPECT_EQ(clears.size(), 1u);
        return clears.empty() ? ClearTextureCommand{} : clears.back();
    }

    // Mapping a staging texture after a copy into it submits everything recorded before.
    void flush() {
        ID3D11Texture2D* staging = create_texture(staging_desc(DXGI_FORMAT_R8_UNORM));
        ID3D11Texture2D* source = create_texture(render_target_desc(DXGI_FORMAT_R8_UNORM));
        context_->CopyResource(staging, source);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_READ, 0, &mapped), S_OK);
        source->Release();
        staging->Release();
    }

    std::uint32_t float_bits(float value) { return std::bit_cast<std::uint32_t>(value); }
};

TEST_F(ContextTest, FloatFormatsClearWithFloatBits) {
    const FLOAT color[4] = {0.25f, -0.5f, 1.5f, 1.0f};
    const ClearTextureCommand command = clear(DXGI_FORMAT_R16G16B16A16_FLOAT, color);
    EXPECT_EQ(command.color.kind, ClearKind::float_value);
    EXPECT_EQ(command.color.bits[0], float_bits(0.25f));
    EXPECT_EQ(command.color.bits[1], float_bits(-0.5f));
}

TEST_F(ContextTest, UintClearsSaturateAndTruncate) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const FLOAT color[4] = {nan, -3.0f, infinity, 7.9f};
    const ClearTextureCommand command = clear(DXGI_FORMAT_R32G32B32A32_UINT, color);
    EXPECT_EQ(command.color.kind, ClearKind::uint_value);
    EXPECT_EQ(command.color.bits[0], 0u);
    EXPECT_EQ(command.color.bits[1], 0u);
    EXPECT_EQ(command.color.bits[2], 0xffffffffu);
    EXPECT_EQ(command.color.bits[3], 7u);
}

TEST_F(ContextTest, SintClearsSaturate) {
    const float infinity = std::numeric_limits<float>::infinity();
    const FLOAT color[4] = {-infinity, infinity, -7.9f, 3e9f};
    const ClearTextureCommand command = clear(DXGI_FORMAT_R32G32B32A32_SINT, color);
    EXPECT_EQ(command.color.kind, ClearKind::sint_value);
    EXPECT_EQ(static_cast<std::int32_t>(command.color.bits[0]), INT32_MIN);
    EXPECT_EQ(static_cast<std::int32_t>(command.color.bits[1]), INT32_MAX);
    EXPECT_EQ(static_cast<std::int32_t>(command.color.bits[2]), -7);
    EXPECT_EQ(static_cast<std::int32_t>(command.color.bits[3]), INT32_MAX);
}

TEST_F(ContextTest, CopyResourceReadsEverySubresourceIntoTheStagingLayout) {
    D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    desc.MipLevels = 2;
    desc.ArraySize = 2;
    ID3D11Texture2D* source = create_texture(desc);
    D3D11_TEXTURE2D_DESC staging = staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    staging.MipLevels = 2;
    staging.ArraySize = 2;
    ID3D11Texture2D* destination = create_texture(staging);
    context_->CopyResource(destination, source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ASSERT_EQ(context_->Map(destination, 3, D3D11_MAP_READ, 0, &mapped), S_OK);
    EXPECT_EQ(fake_bridge::state().submits, 1);
    const auto copies = fake_bridge::commands_of<CopyTextureToBufferCommand>();
    ASSERT_EQ(copies.size(), 4u);
    EXPECT_EQ(copies[1].region.mip, 1u);
    EXPECT_EQ(copies[1].region.slice, 0u);
    EXPECT_EQ(copies[1].region.offset, 256u * 8);
    EXPECT_EQ(copies[1].region.extent.width, 4u);
    EXPECT_EQ(copies[1].region.row_pitch, 256u);
    EXPECT_EQ(copies[3].region.slice, 1u);
    EXPECT_EQ(copies[3].region.offset, 256u * 8 + 256 * 4 + 256 * 8);
    destination->Release();
    source->Release();
}

TEST_F(ContextTest, IncompatibleCopiesRecordNothing) {
    ID3D11Texture2D* source = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    D3D11_TEXTURE2D_DESC wide = staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    wide.Width = 16;
    ID3D11Texture2D* wrong_size = create_texture(wide);
    ID3D11Texture2D* wrong_family = create_texture(staging_desc(DXGI_FORMAT_R16G16_FLOAT));
    context_->CopyResource(wrong_size, source);
    context_->CopyResource(wrong_family, source);
    context_->CopyResource(source, source);
    context_->CopyResource(nullptr, source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ASSERT_EQ(context_->Map(wrong_size, 0, D3D11_MAP_READ, 0, &mapped), S_OK);
    EXPECT_TRUE(fake_bridge::commands_of<CopyTextureToBufferCommand>().empty());
    wrong_family->Release();
    wrong_size->Release();
    source->Release();
}

// Formats outside any typeless family copy only to the same format: R11G11B10_FLOAT into a B5G6R5
// staging texture would write twice its size.
TEST_F(ContextTest, FormatsWithoutAFamilyCopyOnlyToThemselves) {
    ID3D11Texture2D* source = create_texture(render_target_desc(DXGI_FORMAT_R11G11B10_FLOAT));
    ID3D11Texture2D* narrower = create_texture(staging_desc(DXGI_FORMAT_B5G6R5_UNORM));
    ID3D11Texture2D* same = create_texture(staging_desc(DXGI_FORMAT_R11G11B10_FLOAT));
    context_->CopyResource(narrower, source);
    context_->CopyResource(same, source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ASSERT_EQ(context_->Map(same, 0, D3D11_MAP_READ, 0, &mapped), S_OK);
    const auto copies = fake_bridge::commands_of<CopyTextureToBufferCommand>();
    ASSERT_EQ(copies.size(), 1u);
    EXPECT_EQ(copies[0].region.row_pitch, 256u);
    same->Release();
    narrower->Release();
    source->Release();
}

TEST_F(ContextTest, MapRejectsInvalidRequests) {
    ID3D11Texture2D* target = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    ID3D11Texture2D* staging = create_texture(staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    D3D11_MAPPED_SUBRESOURCE mapped{};
    EXPECT_EQ(context_->Map(target, 0, D3D11_MAP_READ, 0, &mapped), E_INVALIDARG);
    EXPECT_EQ(context_->Map(staging, 1, D3D11_MAP_READ, 0, &mapped), E_INVALIDARG);
    EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_WRITE, 0, &mapped), E_INVALIDARG);
    EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), E_INVALIDARG);
    EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_READ, 0x1, &mapped), E_INVALIDARG);
    EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_READ, 0, nullptr), E_INVALIDARG);
    EXPECT_EQ(context_->Map(nullptr, 0, D3D11_MAP_READ, 0, &mapped), E_INVALIDARG);
    EXPECT_EQ(mapped.pData, nullptr);
    EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped), S_OK);
    staging->Release();
    target->Release();
}

TEST_F(ContextTest, MapAfterAStreamErrorReportsDeviceRemoved) {
    ID3D11Texture2D* source = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    ID3D11Texture2D* staging = create_texture(staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    context_->CopyResource(staging, source);
    fake_bridge::fail_stream();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    EXPECT_EQ(context_->Map(staging, 0, D3D11_MAP_READ, 0, &mapped), DXGI_ERROR_DEVICE_REMOVED);
    staging->Release();
    source->Release();
}

TEST_F(ContextTest, GetImmediateContextAddsAReference) {
    ID3D11DeviceContext* context = nullptr;
    device_->GetImmediateContext(&context);
    EXPECT_EQ(context, context_);
    EXPECT_EQ(context->Release(), 1u);
}

} // namespace
