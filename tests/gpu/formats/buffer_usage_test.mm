#include "format_fixture.hpp"

#include <algorithm>
#include <utility>

// Buffer usages: texture buffers are created over a Metal buffer for every format claiming
// Support::buffer; vertex and index buffer claims must name an MTLVertexFormat or MTLIndexType.
namespace directmetal::formats::gpu_testing {
namespace {

constexpr NSUInteger texture_buffer_width = 64;

// Apple's MTLVertexFormat for each DXGI format Direct3D allows in vertex buffers.
constexpr std::pair<DxgiFormat, MTLVertexFormat> vertex_formats[] = {
    {2, MTLVertexFormatFloat4},
    {3, MTLVertexFormatUInt4},
    {4, MTLVertexFormatInt4},
    {6, MTLVertexFormatFloat3},
    {7, MTLVertexFormatUInt3},
    {8, MTLVertexFormatInt3},
    {10, MTLVertexFormatHalf4},
    {11, MTLVertexFormatUShort4Normalized},
    {12, MTLVertexFormatUShort4},
    {13, MTLVertexFormatShort4Normalized},
    {14, MTLVertexFormatShort4},
    {16, MTLVertexFormatFloat2},
    {17, MTLVertexFormatUInt2},
    {18, MTLVertexFormatInt2},
    {24, MTLVertexFormatUInt1010102Normalized},
    {26, MTLVertexFormatFloatRG11B10},
    {28, MTLVertexFormatUChar4Normalized},
    {30, MTLVertexFormatUChar4},
    {31, MTLVertexFormatChar4Normalized},
    {32, MTLVertexFormatChar4},
    {34, MTLVertexFormatHalf2},
    {35, MTLVertexFormatUShort2Normalized},
    {36, MTLVertexFormatUShort2},
    {37, MTLVertexFormatShort2Normalized},
    {38, MTLVertexFormatShort2},
    {41, MTLVertexFormatFloat},
    {42, MTLVertexFormatUInt},
    {43, MTLVertexFormatInt},
    {49, MTLVertexFormatUChar2Normalized},
    {50, MTLVertexFormatUChar2},
    {51, MTLVertexFormatChar2Normalized},
    {52, MTLVertexFormatChar2},
    {54, MTLVertexFormatHalf},
    {56, MTLVertexFormatUShortNormalized},
    {57, MTLVertexFormatUShort},
    {58, MTLVertexFormatShortNormalized},
    {59, MTLVertexFormatShort},
    {61, MTLVertexFormatUCharNormalized},
    {62, MTLVertexFormatUChar},
    {63, MTLVertexFormatCharNormalized},
    {64, MTLVertexFormatChar},
    {87, MTLVertexFormatUChar4Normalized_BGRA},
};

constexpr std::pair<DxgiFormat, MTLIndexType> index_formats[] = {
    {42, MTLIndexTypeUInt32},
    {57, MTLIndexTypeUInt16},
};

template <typename Table> bool is_listed(const Table& table, DxgiFormat format) {
    return std::ranges::any_of(table,
                               [format](const auto& entry) { return entry.first == format; });
}

NSUInteger aligned(NSUInteger size, NSUInteger alignment) {
    return (size + alignment - 1) / alignment * alignment;
}

class BufferUsageTest : public FormatGpuTest,
                        public ::testing::WithParamInterface<const FormatInfo*> {
protected:
    const FormatInfo& info() const { return *GetParam(); }
    Support claimed() const { return support(info().format, features()); }
};

TEST_P(BufferUsageTest, CreatesClaimedTextureBuffer) {
    if (!has_any(claimed(), Support::buffer)) {
        GTEST_SKIP() << "no texture buffer claimed on this device";
    }
    const auto pixel_format = static_cast<MTLPixelFormat>(info().pixel_format);
    const bool is_writable = has_any(claimed(), Support::typed_uav_store | Support::typed_uav_load);
    const MTLTextureUsage usage =
        MTLTextureUsageShaderRead | (is_writable ? MTLTextureUsageShaderWrite : 0);
    const NSUInteger row_bytes =
        aligned(texture_buffer_width * info().bytes_per_block,
                [metal_device() minimumLinearTextureAlignmentForPixelFormat:pixel_format]);
    id<MTLBuffer> buffer = [metal_device() newBufferWithLength:row_bytes
                                                       options:MTLResourceStorageModeShared];
    MTLTextureDescriptor* descriptor =
        [MTLTextureDescriptor textureBufferDescriptorWithPixelFormat:pixel_format
                                                               width:texture_buffer_width
                                                     resourceOptions:buffer.resourceOptions
                                                               usage:usage];
    id<MTLTexture> texture = [buffer newTextureWithDescriptor:descriptor
                                                       offset:0
                                                  bytesPerRow:row_bytes];
    EXPECT_NE(texture, nil);
}

TEST_P(BufferUsageTest, VertexBufferClaimNamesVertexFormat) {
    EXPECT_EQ(has_any(claimed(), Support::vertex_buffer), is_listed(vertex_formats, info().format));
}

TEST_P(BufferUsageTest, IndexBufferClaimNamesIndexType) {
    EXPECT_EQ(has_any(claimed(), Support::index_buffer), is_listed(index_formats, info().format));
}

INSTANTIATE_TEST_SUITE_P(DxgiFormats, BufferUsageTest, ::testing::ValuesIn(every_format()),
                         format_test_name);

} // namespace
} // namespace directmetal::formats::gpu_testing
