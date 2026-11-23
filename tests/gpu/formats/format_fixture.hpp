#pragma once

#ifndef __OBJC__
#error "format_fixture.hpp is for Objective-C++ sources only"
#endif

#import <Metal/Metal.h>

#include "texture_plan.hpp"

#include <directmetal/backend/device.hpp>
#include <directmetal/formats/formats.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <span>
#include <vector>

namespace directmetal::formats {

// Names a format parameter in test output instead of printing its address.
inline void PrintTo(const FormatInfo* info, std::ostream* stream) {
    *stream << info->name;
}

} // namespace directmetal::formats

// One device and queue per test suite, with the format features of the running GPU.
namespace directmetal::formats::gpu_testing {

// Long enough for any test's GPU work; a lost submit returns timeout instead of hanging.
inline constexpr std::chrono::nanoseconds patience = std::chrono::seconds(10);

// Capabilities carry the family and BC support; float32 filtering and the read-write tier are not
// in Capabilities, so they come from the MTLDevice behind the backend device.
DeviceFeatures device_features(const backend::Capabilities& capabilities, id<MTLDevice> device);

// Every DXGI_FORMAT table row, for parameterized tests.
std::vector<const FormatInfo*> every_format();

std::string format_test_name(const ::testing::TestParamInfo<const FormatInfo*>& info);

class FormatGpuTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;

    static backend::Device& device();
    static backend::Queue& queue();
    static id<MTLDevice> metal_device();
    static const DeviceFeatures& features();

    void submit_and_wait();
    std::unique_ptr<backend::Buffer> make_buffer(std::span<const std::byte> contents);
    std::unique_ptr<backend::Buffer> make_filled_buffer(std::uint64_t size, std::byte fill);
    std::unique_ptr<backend::Texture> make_texture(const backend::TextureDesc& desc);
    // Record buffer copies of mip 0 of every slice in layout.
    void record_upload(backend::Buffer& source, backend::Texture& texture,
                       const CopyLayout& layout);
    void record_readback(backend::Texture& texture, const CopyLayout& layout,
                         backend::Buffer& destination);
};

std::span<const std::byte> contents(backend::Buffer& buffer);

// Index of the first differing byte, or the size when they match.
std::size_t first_difference(std::span<const std::byte> actual,
                             std::span<const std::byte> expected);

} // namespace directmetal::formats::gpu_testing
