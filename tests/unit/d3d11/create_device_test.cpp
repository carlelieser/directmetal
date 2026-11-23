#include "fake_bridge.hpp"

#include <d3d11.h>

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {

struct Request {
    IDXGIAdapter* adapter = nullptr;
    D3D_DRIVER_TYPE driver = D3D_DRIVER_TYPE_HARDWARE;
    HMODULE software = nullptr;
    std::vector<D3D_FEATURE_LEVEL> levels;
    UINT sdk_version = D3D11_SDK_VERSION;
};

struct Created {
    HRESULT result;
    D3D_FEATURE_LEVEL level;
    ID3D11Device* device;
};

Created create(const Request& request) {
    Created created{E_FAIL, D3D_FEATURE_LEVEL{}, nullptr};
    created.result =
        D3D11CreateDevice(request.adapter, request.driver, request.software, 0,
                          request.levels.empty() ? nullptr : request.levels.data(),
                          static_cast<UINT>(request.levels.size()), request.sdk_version,
                          &created.device, &created.level, nullptr);
    return created;
}

class CreateDeviceTest : public ::testing::Test {
protected:
    void SetUp() override { fake_bridge::reset(); }
};

TEST_F(CreateDeviceTest, DefaultLevelsGiveFeatureLevel11AndReleaseToZero) {
    Created created = create({});
    ASSERT_EQ(created.result, S_OK);
    EXPECT_EQ(created.level, D3D_FEATURE_LEVEL_11_0);
    EXPECT_EQ(created.device->Release(), 0u);
    EXPECT_EQ(fake_bridge::state().devices_destroyed, 1);
}

TEST_F(CreateDeviceTest, AdapterIsInvalidWithoutDxgi) {
    int adapter_storage = 0;
    Request request;
    request.adapter = reinterpret_cast<IDXGIAdapter*>(&adapter_storage);
    const Created created = create(request);
    EXPECT_EQ(created.result, E_INVALIDARG);
    EXPECT_EQ(created.device, nullptr);
    EXPECT_EQ(fake_bridge::state().devices_created, 0);
}

struct DriverCase {
    D3D_DRIVER_TYPE driver;
    bool has_software;
    HRESULT expected;
};

class DriverRuleTest : public CreateDeviceTest, public ::testing::WithParamInterface<DriverCase> {};

TEST_P(DriverRuleTest, ReturnsDirect3DResult) {
    int module_storage = 0;
    Request request;
    request.driver = GetParam().driver;
    request.software =
        GetParam().has_software ? reinterpret_cast<HMODULE>(&module_storage) : nullptr;
    const Created created = create(request);
    EXPECT_EQ(created.result, GetParam().expected);
    EXPECT_EQ(created.device, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    Drivers, DriverRuleTest,
    ::testing::Values(DriverCase{D3D_DRIVER_TYPE_UNKNOWN, false, E_INVALIDARG},
                      DriverCase{D3D_DRIVER_TYPE_WARP, false, DXGI_ERROR_UNSUPPORTED},
                      DriverCase{D3D_DRIVER_TYPE_REFERENCE, false, DXGI_ERROR_UNSUPPORTED},
                      DriverCase{D3D_DRIVER_TYPE_NULL, false, DXGI_ERROR_UNSUPPORTED},
                      DriverCase{D3D_DRIVER_TYPE_SOFTWARE, false, E_INVALIDARG},
                      DriverCase{D3D_DRIVER_TYPE_SOFTWARE, true, DXGI_ERROR_UNSUPPORTED},
                      DriverCase{D3D_DRIVER_TYPE_HARDWARE, true, E_INVALIDARG},
                      // 6 is unassigned but inside the enum's value range.
                      DriverCase{static_cast<D3D_DRIVER_TYPE>(6), false, E_INVALIDARG}));

TEST_F(CreateDeviceTest, WrongSdkVersionIsInvalid) {
    Request request;
    request.sdk_version = D3D11_SDK_VERSION + 1;
    EXPECT_EQ(create(request).result, E_INVALIDARG);
}

TEST_F(CreateDeviceTest, NullLevelsWithACountAreInvalid) {
    ID3D11Device* device = nullptr;
    EXPECT_EQ(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 1,
                                D3D11_SDK_VERSION, &device, nullptr, nullptr),
              E_INVALIDARG);
    EXPECT_EQ(device, nullptr);
}

struct LevelCase {
    std::vector<D3D_FEATURE_LEVEL> levels;
    HRESULT expected;
    D3D_FEATURE_LEVEL chosen;
};

class FeatureLevelTest : public CreateDeviceTest,
                         public ::testing::WithParamInterface<LevelCase> {};

TEST_P(FeatureLevelTest, NegotiatesFirstSupportedLevel) {
    Request request;
    request.levels = GetParam().levels;
    Created created = create(request);
    EXPECT_EQ(created.result, GetParam().expected);
    if (SUCCEEDED(created.result)) {
        EXPECT_EQ(created.level, GetParam().chosen);
        EXPECT_EQ(created.device->Release(), 0u);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Levels, FeatureLevelTest,
    ::testing::Values(
        LevelCase{{D3D_FEATURE_LEVEL_11_0}, S_OK, D3D_FEATURE_LEVEL_11_0},
        LevelCase{{D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_11_0}, S_OK, D3D_FEATURE_LEVEL_11_0},
        LevelCase{{D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_9_3}, DXGI_ERROR_UNSUPPORTED, {}},
        LevelCase{{D3D_FEATURE_LEVEL_12_0}, DXGI_ERROR_UNSUPPORTED, {}},
        LevelCase{
            {D3D_FEATURE_LEVEL_11_0, static_cast<D3D_FEATURE_LEVEL>(0x1234)}, E_INVALIDARG, {}}));

TEST_F(CreateDeviceTest, NoOutputsReturnSFalseAndDestroyTheDevice) {
    EXPECT_EQ(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                D3D11_SDK_VERSION, nullptr, nullptr, nullptr),
              S_FALSE);
    EXPECT_EQ(fake_bridge::state().devices_destroyed, 1);
}

TEST_F(CreateDeviceTest, ContextAloneKeepsTheDeviceAlive) {
    ID3D11DeviceContext* context = nullptr;
    ASSERT_EQ(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                D3D11_SDK_VERSION, nullptr, nullptr, &context),
              S_OK);
    EXPECT_EQ(fake_bridge::state().devices_destroyed, 0);
    EXPECT_EQ(context->Release(), 0u);
    EXPECT_EQ(fake_bridge::state().devices_destroyed, 1);
}

TEST_F(CreateDeviceTest, BridgeInitializeFailureIsUnsupported) {
    fake_bridge::state().initialize_status = directmetal::bridge::status_dll_not_found;
    const Created created = create({});
    EXPECT_EQ(created.result, DXGI_ERROR_UNSUPPORTED);
    EXPECT_EQ(created.device, nullptr);
}

TEST_F(CreateDeviceTest, AnswersDeviceButNotDxgiInterfaces) {
    Created created = create({});
    ASSERT_EQ(created.result, S_OK);
    void* found = &created;
    EXPECT_EQ(created.device->QueryInterface(__uuidof(IDXGIDevice), &found), E_NOINTERFACE);
    EXPECT_EQ(found, nullptr);
    ASSERT_EQ(created.device->QueryInterface(__uuidof(ID3D11Device), &found), S_OK);
    EXPECT_EQ(found, created.device);
    EXPECT_EQ(created.device->Release(), 1u);
    EXPECT_EQ(created.device->Release(), 0u);
}

} // namespace
