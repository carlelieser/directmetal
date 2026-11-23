#include "format_fixture.hpp"

#include "resource_access.hpp"

// Formats claiming multisample render targets are created as 4x multisample textures and cleared;
// those also claiming multisample resolve are resolved into a single-sample texture, which reads
// back the cleared value.
namespace directmetal::formats::gpu_testing {
namespace {

using backend::TextureUsage;

constexpr std::uint32_t sample_count = 4;
constexpr backend::Extent3D extent{8, 8, 1};
constexpr backend::SubresourceRange whole{0, 1, 0, 1};
constexpr auto both_aspects = static_cast<backend::ClearAspect>(0x3);

TextureUsage without_writes(TextureUsage usage) {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(usage) &
                                     ~static_cast<std::uint32_t>(TextureUsage::shader_write));
}

class MultisampleTest : public FormatGpuTest,
                        public ::testing::WithParamInterface<const FormatInfo*> {
protected:
    const FormatInfo& info() const { return *GetParam(); }

    // Direct3D has no unordered access to multisample resources.
    backend::TextureDesc multisample_desc(Support claimed) const {
        const bool is_depth = has_any(claimed, Support::depth_stencil);
        return {backend::TextureType::tex2d_ms,
                backend::StorageMode::gpu_private,
                0,
                is_depth ? info().depth_pixel_format : resource_pixel_format(info()),
                extent,
                1,
                1,
                sample_count,
                without_writes(claimed_usage(info(), claimed))};
    }

    void clear(backend::Texture& texture, Support claimed) {
        if (has_any(claimed, Support::depth_stencil)) {
            queue().recorder().clear_depth_stencil(texture, whole, {1.0f, 0, both_aspects, 0});
        } else {
            queue().recorder().clear_texture(texture, whole,
                                             {{}, backend::ClearKind::float_value, {}});
        }
    }

    // Clears the multisample texture again and resolves it in one render pass.
    void resolve(backend::Texture& multisample, backend::Texture& resolved) {
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        MTLRenderPassColorAttachmentDescriptor* color = pass.colorAttachments[0];
        color.texture = backend::resources::metal_texture(multisample);
        color.resolveTexture = backend::resources::metal_texture(resolved);
        color.loadAction = MTLLoadActionClear;
        color.clearColor = MTLClearColorMake(0, 0, 0, 0);
        color.storeAction = MTLStoreActionMultisampleResolve;
        id<MTLCommandBuffer> commands = [[metal_device() newCommandQueue] commandBuffer];
        [[commands renderCommandEncoderWithDescriptor:pass] endEncoding];
        [commands commit];
        [commands waitUntilCompleted];
        EXPECT_EQ(commands.status, MTLCommandBufferStatusCompleted);
    }

    void expect_resolves(backend::Texture& multisample, Support claimed) {
        backend::TextureDesc desc = multisample_desc(claimed);
        desc.type = backend::TextureType::tex2d;
        desc.sample_count = 1;
        desc.usage = claimed_usage(info(), claimed);
        const auto resolved = make_texture(desc);
        ASSERT_NE(resolved, nullptr);
        const PlannedTexture planned{"resolved", desc, Check::color_clear,
                                     copy_layout(info(), desc)};
        const auto upload = make_buffer(upload_pattern(planned));
        const auto readback = make_filled_buffer(planned.layout.total_bytes(), std::byte{0xcd});
        record_upload(*upload, *resolved, planned.layout);
        submit_and_wait();
        resolve(multisample, *resolved);
        record_readback(*resolved, planned.layout, *readback);
        submit_and_wait();
        const std::vector<std::byte> expected = expected_readback(planned);
        EXPECT_EQ(first_difference(contents(*readback), expected), expected.size());
    }
};

TEST_P(MultisampleTest, ClearsAndResolvesClaimedMultisampleTargets) {
    const Support claimed = support(info().format, features());
    if (!has_any(claimed, Support::multisample_render_target)) {
        GTEST_SKIP() << "no multisample render target claimed on this device";
    }
    ASSERT_TRUE([metal_device() supportsTextureSampleCount:sample_count]);
    const auto multisample = make_texture(multisample_desc(claimed));
    ASSERT_NE(multisample, nullptr);
    multisample->set_label(info().name);
    clear(*multisample, claimed);
    submit_and_wait();
    if (has_any(claimed, Support::multisample_resolve)) {
        expect_resolves(*multisample, claimed);
    }
}

INSTANTIATE_TEST_SUITE_P(DxgiFormats, MultisampleTest, ::testing::ValuesIn(every_format()),
                         format_test_name);

} // namespace
} // namespace directmetal::formats::gpu_testing
