#include "format_fixture.hpp"

// Every format whose support() on the running GPU claims a texture dimension is created with
// exactly its claimed usages in each claimed dimension, filled with a pattern, cleared where it
// claims render-target or depth-stencil use, and copied back to a buffer. Metal validation aborts
// on anything it rejects.
namespace directmetal::formats::gpu_testing {
namespace {

constexpr auto both_aspects = static_cast<backend::ClearAspect>(0x3);
constexpr backend::DepthStencilClear depth_one{1.0f, stencil_clear_value, both_aspects, 0};
constexpr std::byte unwritten{0xcd};

struct Recorded {
    PlannedTexture planned;
    std::unique_ptr<backend::Texture> texture;
    std::unique_ptr<backend::Buffer> upload;
    std::unique_ptr<backend::Buffer> readback;
};

backend::ClearKind clear_kind(ComponentKind kind) {
    switch (kind) {
    case ComponentKind::uint:
        return backend::ClearKind::uint_value;
    case ComponentKind::sint:
        return backend::ClearKind::sint_value;
    default:
        return backend::ClearKind::float_value;
    }
}

class ClaimedTextureTest : public FormatGpuTest,
                           public ::testing::WithParamInterface<const FormatInfo*> {
protected:
    const FormatInfo& info() const { return *GetParam(); }

    void record_clear(const PlannedTexture& planned, backend::Texture& texture) {
        const backend::SubresourceRange range{0, 1, 0, planned.layout.slice_count};
        if (planned.check == Check::color_clear) {
            const backend::ClearColor zero{{}, clear_kind(info().kind), {}};
            queue().recorder().clear_texture(texture, range, zero);
        } else if (planned.check == Check::depth_clear) {
            queue().recorder().clear_depth_stencil(texture, range, depth_one);
        }
    }

    // Null when the texture could not be created; the failure is already reported.
    std::unique_ptr<Recorded> record(const PlannedTexture& planned) {
        auto recorded = std::make_unique<Recorded>(Recorded{
            planned, make_texture(planned.desc), make_buffer(upload_pattern(planned)), nullptr});
        recorded->readback = make_filled_buffer(planned.layout.total_bytes(), unwritten);
        if (recorded->texture == nullptr) {
            return nullptr;
        }
        recorded->texture->set_label(info().name);
        record_upload(*recorded->upload, *recorded->texture, planned.layout);
        record_clear(planned, *recorded->texture);
        record_readback(*recorded->texture, planned.layout, *recorded->readback);
        return recorded;
    }

    void expect_readback(Recorded& recorded) {
        const std::vector<std::byte> expected = expected_readback(recorded.planned);
        const std::span<const std::byte> actual = contents(*recorded.readback);
        const std::size_t index = first_difference(actual, expected);
        EXPECT_EQ(index, expected.size())
            << recorded.planned.label << " texture differs at byte " << index << ": read "
            << std::to_integer<int>(actual[index]) << ", expected "
            << std::to_integer<int>(expected[index]);
    }
};

TEST_P(ClaimedTextureTest, CreatesClearsAndCopiesClaimedDimensions) {
    const Support claimed = support(info().format, features());
    const std::vector<PlannedTexture> planned = plan_textures(info(), claimed);
    if (planned.empty()) {
        GTEST_SKIP() << "no texture dimension claimed on this device";
    }
    std::vector<std::unique_ptr<Recorded>> recorded;
    for (const PlannedTexture& texture : planned) {
        ASSERT_NE(texture.desc.usage, backend::TextureUsage::none) << texture.label;
        recorded.push_back(record(texture));
        ASSERT_NE(recorded.back(), nullptr) << texture.label;
    }
    submit_and_wait();
    for (const auto& texture : recorded) {
        expect_readback(*texture);
    }
}

INSTANTIATE_TEST_SUITE_P(DxgiFormats, ClaimedTextureTest, ::testing::ValuesIn(every_format()),
                         format_test_name);

} // namespace
} // namespace directmetal::formats::gpu_testing
