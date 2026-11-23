#include "d3d11_fixture.hpp"

#include <cstdint>
#include <cstring>

namespace {

using ObjectMethodsTest = D3d11Test;

constexpr GUID test_guid = {
    0x6a1d1c55, 0x2f0e, 0x4c8b, {0x9a, 0x51, 0x13, 0x7e, 0x22, 0x40, 0x0d, 0x99}};

TEST_F(ObjectMethodsTest, DeviceReportsItsCreationSettings) {
    EXPECT_EQ(device_->GetFeatureLevel(), D3D_FEATURE_LEVEL_11_0);
    EXPECT_EQ(device_->GetCreationFlags(), 0u);
    EXPECT_EQ(device_->GetDeviceRemovedReason(), S_OK);
}

TEST_F(ObjectMethodsTest, CreationFlagsAreKept) {
    ID3D11Device* device = nullptr;
    const UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
    ASSERT_EQ(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                D3D11_SDK_VERSION, &device, nullptr, nullptr),
              S_OK);
    EXPECT_EQ(device->GetCreationFlags(), flags);
    EXPECT_EQ(device->Release(), 0u);
}

TEST_F(ObjectMethodsTest, StreamErrorRemovesTheDevice) {
    fake_bridge::fail_stream();
    EXPECT_EQ(device_->GetDeviceRemovedReason(), DXGI_ERROR_DEVICE_REMOVED);
}

TEST_F(ObjectMethodsTest, ChildrenReturnTheirDeviceWithAReference) {
    ID3D11Texture2D* texture = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    ID3D11Device* device = nullptr;
    texture->GetDevice(&device);
    EXPECT_EQ(device, device_);
    // Fixture, context, texture and GetDevice.
    EXPECT_EQ(device->Release(), 3u);
    device = nullptr;
    context_->GetDevice(&device);
    EXPECT_EQ(device, device_);
    device->Release();
    texture->Release();
}

TEST_F(ObjectMethodsTest, ContextIsImmediate) {
    EXPECT_EQ(context_->GetType(), D3D11_DEVICE_CONTEXT_IMMEDIATE);
}

TEST_F(ObjectMethodsTest, TextureReportsItsResolvedDescAndType) {
    D3D11_TEXTURE2D_DESC desc = staging_desc(DXGI_FORMAT_R16G16B16A16_FLOAT);
    desc.MipLevels = 0;
    ID3D11Texture2D* texture = create_texture(desc);
    D3D11_TEXTURE2D_DESC reported{};
    texture->GetDesc(&reported);
    EXPECT_EQ(reported.MipLevels, 4u);
    EXPECT_EQ(reported.Format, DXGI_FORMAT_R16G16B16A16_FLOAT);
    EXPECT_EQ(reported.Usage, D3D11_USAGE_STAGING);
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    texture->GetType(&dimension);
    EXPECT_EQ(dimension, D3D11_RESOURCE_DIMENSION_TEXTURE2D);
    texture->Release();
}

TEST_F(ObjectMethodsTest, ViewReportsItsResourceAndDesc) {
    D3D11_TEXTURE2D_DESC desc = render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM);
    desc.MipLevels = 2;
    ID3D11Texture2D* texture = create_texture(desc);
    D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
    view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipSlice = 1;
    ID3D11RenderTargetView* view = nullptr;
    ASSERT_EQ(device_->CreateRenderTargetView(texture, &view_desc, &view), S_OK);
    D3D11_RENDER_TARGET_VIEW_DESC reported{};
    view->GetDesc(&reported);
    EXPECT_EQ(reported.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(reported.Texture2D.MipSlice, 1u);
    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    EXPECT_EQ(resource, static_cast<ID3D11Resource*>(texture));
    EXPECT_EQ(resource->Release(), 1u);
    view->Release();
    texture->Release();
}

TEST_F(ObjectMethodsTest, ChildPrivateDataRoundTrips) {
    ID3D11Texture2D* texture = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    const std::uint32_t value = 0x12345678;
    ASSERT_EQ(texture->SetPrivateData(test_guid, sizeof(value), &value), S_OK);
    std::uint32_t read = 0;
    UINT size = sizeof(read);
    EXPECT_EQ(texture->GetPrivateData(test_guid, &size, &read), S_OK);
    EXPECT_EQ(read, value);
    size = 1;
    EXPECT_EQ(texture->GetPrivateData(test_guid, &size, &read), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(texture->GetPrivateData(test_guid, nullptr, &read), E_INVALIDARG);
    UINT missing_size = 4;
    EXPECT_EQ(context_->GetPrivateData(test_guid, &missing_size, &read), DXGI_ERROR_NOT_FOUND);
    ASSERT_EQ(texture->SetPrivateDataInterface(test_guid, device_), S_OK);
    // The store's reference on the device, the texture's and the fixture's and context's.
    EXPECT_EQ(device_->AddRef(), 5u);
    device_->Release();
    texture->Release();
    EXPECT_EQ(device_->AddRef(), 3u);
    device_->Release();
}

TEST_F(ObjectMethodsTest, DevicePrivateDataRoundTrips) {
    const char name[] = "device";
    ASSERT_EQ(device_->SetPrivateData(test_guid, sizeof(name), name), S_OK);
    char read[sizeof(name)] = {};
    UINT size = sizeof(read);
    EXPECT_EQ(device_->GetPrivateData(test_guid, &size, read), S_OK);
    EXPECT_STREQ(read, name);
    EXPECT_EQ(device_->SetPrivateData(test_guid, 4, nullptr), E_INVALIDARG);
}

} // namespace
