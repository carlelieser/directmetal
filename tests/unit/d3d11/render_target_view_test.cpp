#include "d3d11_fixture.hpp"

#include <functional>
#include <string>

namespace {

using directmetal::bridge::ClearTextureCommand;

class RenderTargetViewTest : public D3d11Test {
protected:
    void SetUp() override {
        D3d11Test::SetUp();
        D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
        desc.MipLevels = 2;
        desc.ArraySize = 4;
        texture_ = create_texture(desc);
    }

    void TearDown() override {
        if (texture_ != nullptr) {
            texture_->Release();
        }
        D3d11Test::TearDown();
    }

    // Releases everything, so the device flushes its stream to the fake bridge.
    void destroy_device() {
        texture_->Release();
        texture_ = nullptr;
        context_->Release();
        context_ = nullptr;
        EXPECT_EQ(device_->Release(), 0u);
        device_ = nullptr;
    }

    ID3D11Texture2D* texture_ = nullptr;
};

struct ViewCase {
    std::string name;
    std::function<void(D3D11_RENDER_TARGET_VIEW_DESC&)> change;
    HRESULT expected;
};

class ViewDescTest : public RenderTargetViewTest, public ::testing::WithParamInterface<ViewCase> {};

D3D11_RENDER_TARGET_VIEW_DESC array_view() {
    D3D11_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray.MipSlice = 1;
    desc.Texture2DArray.FirstArraySlice = 1;
    desc.Texture2DArray.ArraySize = 3;
    return desc;
}

TEST_P(ViewDescTest, ReturnsDirect3DResult) {
    D3D11_RENDER_TARGET_VIEW_DESC desc = array_view();
    GetParam().change(desc);
    int sentinel = 0;
    auto* view = reinterpret_cast<ID3D11RenderTargetView*>(&sentinel);
    EXPECT_EQ(device_->CreateRenderTargetView(texture_, &desc, &view), GetParam().expected);
    if (FAILED(GetParam().expected)) {
        EXPECT_EQ(view, nullptr);
    } else {
        view->Release();
    }
}

INSTANTIATE_TEST_SUITE_P(
    Views, ViewDescTest,
    ::testing::Values(
        ViewCase{"Array", [](D3D11_RENDER_TARGET_VIEW_DESC&) {}, S_OK},
        ViewCase{"UnknownFormatIsTheResources",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) { desc.Format = DXGI_FORMAT_UNKNOWN; },
                 S_OK},
        ViewCase{"MipOutOfRange",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) { desc.Texture2DArray.MipSlice = 2; },
                 E_INVALIDARG},
        ViewCase{"SlicesOutOfRange",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) { desc.Texture2DArray.ArraySize = 4; },
                 E_INVALIDARG},
        ViewCase{"NoSlices",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) { desc.Texture2DArray.ArraySize = 0; },
                 E_INVALIDARG},
        ViewCase{"SingleSliceDimensionOnAnArray",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) {
                     desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                 },
                 E_INVALIDARG},
        ViewCase{"BufferDimension",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) {
                     desc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
                 },
                 E_INVALIDARG},
        ViewCase{"SameFamilyOnATypedResource",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) {
                     desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                 },
                 E_INVALIDARG},
        ViewCase{"IncompatibleFormat",
                 [](D3D11_RENDER_TARGET_VIEW_DESC& desc) { desc.Format = DXGI_FORMAT_R32_FLOAT; },
                 E_INVALIDARG}),
    [](const ::testing::TestParamInfo<ViewCase>& info) { return info.param.name; });

TEST_F(RenderTargetViewTest, NullDescViewsMipZeroOfEverySlice) {
    ID3D11RenderTargetView* view = nullptr;
    ASSERT_EQ(device_->CreateRenderTargetView(texture_, nullptr, &view), S_OK);
    const FLOAT color[4] = {0, 0, 0, 0};
    context_->ClearRenderTargetView(view, color);
    view->Release();
    destroy_device();
    const auto clears = fake_bridge::commands_of<ClearTextureCommand>();
    ASSERT_EQ(clears.size(), 1u);
    EXPECT_EQ(clears[0].range.first_mip, 0u);
    EXPECT_EQ(clears[0].range.mip_count, 1u);
    EXPECT_EQ(clears[0].range.first_slice, 0u);
    EXPECT_EQ(clears[0].range.slice_count, 4u);
}

TEST_F(RenderTargetViewTest, NullOutputValidatesWithoutCreating) {
    const D3D11_RENDER_TARGET_VIEW_DESC desc = array_view();
    EXPECT_EQ(device_->CreateRenderTargetView(texture_, &desc, nullptr), S_FALSE);
}

TEST_F(RenderTargetViewTest, StagingAndNullResourcesAreInvalid) {
    ID3D11Texture2D* staging = create_texture(staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    ID3D11RenderTargetView* view = nullptr;
    EXPECT_EQ(device_->CreateRenderTargetView(staging, nullptr, &view), E_INVALIDARG);
    EXPECT_EQ(device_->CreateRenderTargetView(nullptr, nullptr, &view), E_INVALIDARG);
    EXPECT_EQ(view, nullptr);
    staging->Release();
}

TEST_F(RenderTargetViewTest, ViewKeepsItsTextureAlive) {
    ID3D11RenderTargetView* view = nullptr;
    ASSERT_EQ(device_->CreateRenderTargetView(texture_, nullptr, &view), S_OK);
    EXPECT_EQ(texture_->Release(), 0u);
    texture_ = nullptr;
    const FLOAT color[4] = {0, 0, 0, 0};
    context_->ClearRenderTargetView(view, color);
    EXPECT_EQ(view->Release(), 0u);
    EXPECT_EQ(fake_bridge::commands_of<ClearTextureCommand>().size(), 0u);
    context_->Release();
    context_ = nullptr;
    EXPECT_EQ(device_->Release(), 0u);
    device_ = nullptr;
    EXPECT_EQ(fake_bridge::commands_of<ClearTextureCommand>().size(), 1u);
    EXPECT_EQ(fake_bridge::commands_of<directmetal::bridge::DestroyObjectCommand>().size(), 1u);
}

} // namespace
