#pragma once

#include "fake_bridge.hpp"

#include <d3d11.h>

#include <gtest/gtest.h>

#include <array>

// A device on the fake bridge, released at the end of each test.
class D3d11Test : public ::testing::Test {
protected:
    void SetUp() override {
        fake_bridge::reset();
        const std::array levels = {D3D_FEATURE_LEVEL_11_0};
        ASSERT_EQ(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels.data(),
                                    static_cast<UINT>(levels.size()), D3D11_SDK_VERSION, &device_,
                                    nullptr, &context_),
                  S_OK);
    }

    void TearDown() override {
        if (context_ != nullptr) {
            context_->Release();
        }
        if (device_ != nullptr) {
            EXPECT_EQ(device_->Release(), 0u);
        }
    }

    static D3D11_TEXTURE2D_DESC render_target_desc(DXGI_FORMAT format) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 8;
        desc.Height = 8;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        return desc;
    }

    static D3D11_TEXTURE2D_DESC staging_desc(DXGI_FORMAT format) {
        D3D11_TEXTURE2D_DESC desc = render_target_desc(format);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        return desc;
    }

    ID3D11Texture2D* create_texture(const D3D11_TEXTURE2D_DESC& desc) {
        ID3D11Texture2D* texture = nullptr;
        EXPECT_EQ(device_->CreateTexture2D(&desc, nullptr, &texture), S_OK);
        return texture;
    }

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
};
