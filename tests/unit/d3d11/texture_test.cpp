#include "d3d11_fixture.hpp"

#include <functional>
#include <string>

namespace {

using TextureTest = D3d11Test;

struct DescCase {
    std::string name;
    std::function<void(D3D11_TEXTURE2D_DESC&)> change;
    HRESULT expected;
};

class TextureDescTest : public D3d11Test, public ::testing::WithParamInterface<DescCase> {};

// Every failure leaves the output null, as Direct3D does.
TEST_P(TextureDescTest, ReturnsDirect3DResult) {
    D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    GetParam().change(desc);
    int sentinel = 0;
    auto* texture = reinterpret_cast<ID3D11Texture2D*>(&sentinel);
    EXPECT_EQ(device_->CreateTexture2D(&desc, nullptr, &texture), GetParam().expected);
    if (FAILED(GetParam().expected)) {
        EXPECT_EQ(texture, nullptr);
    } else if (texture != nullptr) {
        texture->Release();
    }
}

std::string case_name(const ::testing::TestParamInfo<DescCase>& info) {
    return info.param.name;
}

INSTANTIATE_TEST_SUITE_P(
    Descs, TextureDescTest,
    ::testing::Values(
        DescCase{"RenderTarget", [](D3D11_TEXTURE2D_DESC&) {}, S_OK},
        DescCase{"FullMipChain", [](D3D11_TEXTURE2D_DESC& desc) { desc.MipLevels = 0; }, S_OK},
        DescCase{"ZeroWidth", [](D3D11_TEXTURE2D_DESC& desc) { desc.Width = 0; }, E_INVALIDARG},
        DescCase{"ZeroHeight", [](D3D11_TEXTURE2D_DESC& desc) { desc.Height = 0; }, E_INVALIDARG},
        DescCase{"TooWide", [](D3D11_TEXTURE2D_DESC& desc) { desc.Width = 16385; }, E_INVALIDARG},
        DescCase{"ZeroArraySize", [](D3D11_TEXTURE2D_DESC& desc) { desc.ArraySize = 0; },
                 E_INVALIDARG},
        DescCase{"TooManyMips", [](D3D11_TEXTURE2D_DESC& desc) { desc.MipLevels = 5; },
                 E_INVALIDARG},
        DescCase{"ZeroSamples", [](D3D11_TEXTURE2D_DESC& desc) { desc.SampleDesc.Count = 0; },
                 E_INVALIDARG},
        DescCase{"SingleSampleQuality",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.SampleDesc.Quality = 1; }, E_INVALIDARG},
        DescCase{"UnknownFormat",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.Format = DXGI_FORMAT_UNKNOWN; },
                 E_INVALIDARG},
        DescCase{"FormatWithoutRenderTarget",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.Format = DXGI_FORMAT_BC1_UNORM; },
                 E_INVALIDARG},
        DescCase{"DefaultWithCpuAccess",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; },
                 E_INVALIDARG},
        DescCase{"StagingWithBind",
                 [](D3D11_TEXTURE2D_DESC& desc) {
                     desc.Usage = D3D11_USAGE_STAGING;
                     desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                 },
                 E_INVALIDARG},
        DescCase{"StagingWithoutCpuAccess",
                 [](D3D11_TEXTURE2D_DESC& desc) {
                     desc.Usage = D3D11_USAGE_STAGING;
                     desc.BindFlags = 0;
                 },
                 E_INVALIDARG},
        DescCase{"DynamicReadAccess",
                 [](D3D11_TEXTURE2D_DESC& desc) {
                     desc.Usage = D3D11_USAGE_DYNAMIC;
                     desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                 },
                 E_INVALIDARG},
        DescCase{"ShaderResourceBind",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE; },
                 E_NOTIMPL},
        DescCase{"StagingWrite",
                 [](D3D11_TEXTURE2D_DESC& desc) {
                     desc.Usage = D3D11_USAGE_STAGING;
                     desc.BindFlags = 0;
                     desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                 },
                 E_NOTIMPL},
        DescCase{"Multisample", [](D3D11_TEXTURE2D_DESC& desc) { desc.SampleDesc.Count = 4; },
                 E_NOTIMPL},
        DescCase{
            "MiscFlags",
            [](D3D11_TEXTURE2D_DESC& desc) { desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS; },
            E_NOTIMPL},
        DescCase{"Typeless",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS; },
                 E_NOTIMPL},
        DescCase{"SubstitutedFormat",
                 [](D3D11_TEXTURE2D_DESC& desc) { desc.Format = DXGI_FORMAT_B8G8R8X8_UNORM; },
                 E_NOTIMPL}),
    case_name);

TEST_F(TextureTest, NullDescIsInvalid) {
    ID3D11Texture2D* texture = nullptr;
    EXPECT_EQ(device_->CreateTexture2D(nullptr, nullptr, &texture), E_INVALIDARG);
}

TEST_F(TextureTest, InitialDataIsNotImplemented) {
    const D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    const D3D11_SUBRESOURCE_DATA data{};
    ID3D11Texture2D* texture = nullptr;
    EXPECT_EQ(device_->CreateTexture2D(&desc, &data, &texture), E_NOTIMPL);
}

TEST_F(TextureTest, NullOutputValidatesWithoutCreating) {
    const D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(device_->CreateTexture2D(&desc, nullptr, nullptr), S_FALSE);
    D3D11_TEXTURE2D_DESC invalid = desc;
    invalid.Width = 0;
    EXPECT_EQ(device_->CreateTexture2D(&invalid, nullptr, nullptr), E_INVALIDARG);
    EXPECT_TRUE(fake_bridge::state().textures.empty());
}

TEST_F(TextureTest, RenderTargetBecomesAMetalTexture) {
    D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_B8G8R8A8_UNORM);
    desc.MipLevels = 0;
    desc.ArraySize = 3;
    ID3D11Texture2D* texture = create_texture(desc);
    ASSERT_EQ(fake_bridge::state().textures.size(), 1u);
    const directmetal::backend::TextureDesc& created = fake_bridge::state().textures[0];
    EXPECT_EQ(created.type, directmetal::backend::TextureType::tex2d_array);
    EXPECT_EQ(created.format, directmetal::backend::PixelFormat::bgra8_unorm);
    EXPECT_EQ(created.mip_levels, 4u);
    EXPECT_EQ(created.array_length, 3u);
    EXPECT_EQ(created.usage, directmetal::backend::TextureUsage::render_target);
    EXPECT_EQ(texture->Release(), 0u);
}

TEST_F(TextureTest, ChildrenHoldDeviceReferences) {
    ID3D11Texture2D* texture = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    // The fixture's device reference, the context's and the texture's.
    EXPECT_EQ(device_->AddRef(), 4u);
    EXPECT_EQ(device_->Release(), 3u);
    EXPECT_EQ(texture->Release(), 0u);
    EXPECT_EQ(device_->AddRef(), 3u);
    EXPECT_EQ(device_->Release(), 2u);
}

TEST_F(TextureTest, ReleasingADefaultTextureDestroysItOnTheStream) {
    ID3D11Texture2D* texture = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    texture->Release();
    context_->Release();
    context_ = nullptr;
    device_->Release();
    device_ = nullptr;
    EXPECT_EQ(fake_bridge::commands_of<directmetal::bridge::DestroyObjectCommand>().size(), 1u);
}

// Feature level 11 resources are at most 2048 MB; layout arithmetic must not wrap at 4 GiB.
TEST_F(TextureTest, ResourcesOver2048MegabytesAreOutOfMemory) {
    D3D11_TEXTURE2D_DESC staging = staging_desc(DXGI_FORMAT_R32G32B32A32_FLOAT);
    staging.Width = 16384;
    staging.Height = 16384;
    ID3D11Texture2D* texture = nullptr;
    EXPECT_EQ(device_->CreateTexture2D(&staging, nullptr, &texture), E_OUTOFMEMORY);
    EXPECT_EQ(texture, nullptr);
    D3D11_TEXTURE2D_DESC target = render_target_desc(DXGI_FORMAT_R32G32B32A32_FLOAT);
    target.Width = 16384;
    target.Height = 16384;
    EXPECT_EQ(device_->CreateTexture2D(&target, nullptr, &texture), E_OUTOFMEMORY);
    EXPECT_TRUE(fake_bridge::state().imported_sizes.empty());
    EXPECT_TRUE(fake_bridge::state().textures.empty());
    staging.Height = 8192;
    EXPECT_EQ(device_->CreateTexture2D(&staging, nullptr, nullptr), S_FALSE);
    staging.Height = 8193;
    EXPECT_EQ(device_->CreateTexture2D(&staging, nullptr, nullptr), E_OUTOFMEMORY);
}

TEST_F(TextureTest, StagingTextureIsImportedAndReleased) {
    ID3D11Texture2D* texture = create_texture(staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    ASSERT_EQ(fake_bridge::state().imported_sizes.size(), 1u);
    EXPECT_EQ(fake_bridge::state().imported_sizes[0] % (16 * 1024), 0u);
    EXPECT_TRUE(fake_bridge::state().textures.empty());
    EXPECT_EQ(texture->Release(), 0u);
    EXPECT_EQ(fake_bridge::state().memory_releases, 1);
}

} // namespace
