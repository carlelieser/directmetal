#include "d3d11_fixture.hpp"

#include <cstddef>

namespace {

using StagingLayoutTest = D3d11Test;

// Maps subresource `index` of `texture` and returns the mapping.
D3D11_MAPPED_SUBRESOURCE map(ID3D11DeviceContext* context, ID3D11Texture2D* texture, UINT index) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    EXPECT_EQ(context->Map(texture, index, D3D11_MAP_READ, 0, &mapped), S_OK);
    context->Unmap(texture, index);
    return mapped;
}

TEST_F(StagingLayoutTest, RowsAre256ByteAligned) {
    ID3D11Texture2D* texture = create_texture(staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    const D3D11_MAPPED_SUBRESOURCE mapped = map(context_, texture, 0);
    EXPECT_EQ(mapped.RowPitch, 256u);
    EXPECT_EQ(mapped.DepthPitch, 256u * 8);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(mapped.pData) % 256, 0u);
    texture->Release();
}

TEST_F(StagingLayoutTest, SubresourcesFollowInDirect3DOrder) {
    D3D11_TEXTURE2D_DESC desc = staging_desc(DXGI_FORMAT_R32G32B32A32_FLOAT);
    desc.Width = 20;
    desc.Height = 4;
    desc.MipLevels = 2;
    desc.ArraySize = 2;
    ID3D11Texture2D* texture = create_texture(desc);
    const auto* base = static_cast<std::byte*>(map(context_, texture, 0).pData);
    const D3D11_MAPPED_SUBRESOURCE mip1 = map(context_, texture, 1);
    const D3D11_MAPPED_SUBRESOURCE slice1 = map(context_, texture, 2);
    // 20 texels of 16 bytes round up to 512; mip 1 has 10 texels and 2 rows.
    EXPECT_EQ(static_cast<std::byte*>(mip1.pData) - base, 512 * 4);
    EXPECT_EQ(mip1.RowPitch, 256u);
    EXPECT_EQ(mip1.DepthPitch, 512u);
    EXPECT_EQ(static_cast<std::byte*>(slice1.pData) - base, 512 * 4 + 512);
    ASSERT_EQ(fake_bridge::state().imported_sizes.size(), 1u);
    EXPECT_EQ(fake_bridge::state().imported_sizes[0], 16u * 1024);
    texture->Release();
}

TEST_F(StagingLayoutTest, CompressedRowsCountBlocks) {
    D3D11_TEXTURE2D_DESC desc = staging_desc(DXGI_FORMAT_BC1_UNORM);
    desc.Width = 1024;
    desc.Height = 16;
    ID3D11Texture2D* texture = create_texture(desc);
    const D3D11_MAPPED_SUBRESOURCE mapped = map(context_, texture, 0);
    // 256 blocks of 8 bytes per row of blocks, 4 rows of blocks.
    EXPECT_EQ(mapped.RowPitch, 2048u);
    EXPECT_EQ(mapped.DepthPitch, 2048u * 4);
    texture->Release();
}

TEST_F(StagingLayoutTest, ImportSizeIsAlignedForLargeTextures) {
    D3D11_TEXTURE2D_DESC desc = staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    desc.Width = 300;
    desc.Height = 300;
    ID3D11Texture2D* texture = create_texture(desc);
    ASSERT_EQ(fake_bridge::state().imported_sizes.size(), 1u);
    // 1280-byte rows, 300 of them, rounded up to 16 KiB.
    EXPECT_EQ(fake_bridge::state().imported_sizes[0], 24u * 16 * 1024);
    texture->Release();
}

} // namespace
