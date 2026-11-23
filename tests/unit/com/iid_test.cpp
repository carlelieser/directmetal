#include <directmetal/com/object.hpp>

#include <d3d11.h>

#include <gtest/gtest.h>

namespace {

using directmetal::com::iid_of;

// https://learn.microsoft.com/windows/win32/api/unknwn/nn-unknwn-iunknown
constexpr GUID iunknown_iid = {
    0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
// https://learn.microsoft.com/windows/win32/api/d3d11/nn-d3d11-id3d11device
constexpr GUID id3d11device_iid = {
    0xdb6f6ddb, 0xac77, 0x4e88, {0x82, 0x53, 0x81, 0x9d, 0xf9, 0xbb, 0xf1, 0x40}};

TEST(IidTest, ResolvesDocumentedIids) {
    EXPECT_TRUE(IsEqualGUID(iid_of<IUnknown>(), iunknown_iid));
    EXPECT_TRUE(IsEqualGUID(iid_of<ID3D11Device>(), id3d11device_iid));
    EXPECT_TRUE(IsEqualGUID(IID_ID3D11Device, id3d11device_iid));
}

TEST(IidTest, UuidofAcceptsPointerAndReferenceExpressions) {
    ID3D11Device* device = nullptr;

    EXPECT_TRUE(IsEqualGUID(__uuidof(device), id3d11device_iid));
    EXPECT_TRUE(IsEqualGUID(__uuidof(*device), id3d11device_iid));
}

TEST(IidTest, DistinguishesInterfaces) {
    EXPECT_FALSE(IsEqualGUID(iid_of<ID3D11Device>(), iid_of<ID3D11DeviceContext>()));
}

} // namespace
