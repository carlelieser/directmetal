#include "context_commands.hpp"

#include "bridge_device.hpp"
#include "texture.hpp"

namespace directmetal::d3d11 {
namespace {

bridge::BufferTextureRegion readback_region(const Texture2D& destination, const Texture2D& source,
                                            UINT subresource) noexcept {
    const SubresourceLayout& layout = destination.layout().at(subresource);
    const UINT mip_levels = source.desc().MipLevels;
    bridge::BufferTextureRegion region{};
    region.buffer = destination.handle();
    region.offset = layout.offset;
    region.row_pitch = layout.row_pitch;
    region.slice_pitch = layout.depth_pitch;
    region.texture = source.handle();
    region.mip = subresource % mip_levels;
    region.slice = subresource / mip_levels;
    region.extent = {layout.width, layout.height, 1};
    return region;
}

// Family 0 means the format has no typeless family and copies only to itself.
bool is_same_format_family(DXGI_FORMAT destination, DXGI_FORMAT source) noexcept {
    if (destination == source) {
        return true;
    }
    const formats::FormatInfo* destination_format =
        formats::find(static_cast<std::uint32_t>(destination));
    const formats::FormatInfo* source_format = formats::find(static_cast<std::uint32_t>(source));
    const formats::DxgiFormat family = source_format->typeless_family;
    return family != 0 && destination_format->typeless_family == family;
}

} // namespace

bool is_copy_compatible(const D3D11_TEXTURE2D_DESC& destination,
                        const D3D11_TEXTURE2D_DESC& source) noexcept {
    const bool is_same_size =
        destination.Width == source.Width && destination.Height == source.Height;
    const bool is_same_layout =
        destination.MipLevels == source.MipLevels && destination.ArraySize == source.ArraySize;
    if (!is_same_size || !is_same_layout) {
        return false;
    }
    const bool is_same_sampling = destination.SampleDesc.Count == source.SampleDesc.Count &&
                                  destination.SampleDesc.Quality == source.SampleDesc.Quality;
    return is_same_sampling && is_same_format_family(destination.Format, source.Format);
}

std::uint64_t record_readback(BridgeDevice& bridge, const Texture2D& destination,
                              const Texture2D& source) {
    std::uint64_t value = 0;
    for (UINT subresource = 0; subresource < source.subresource_count(); ++subresource) {
        value = bridge.record(
            bridge::CopyTextureToBufferCommand{readback_region(destination, source, subresource)});
    }
    return value;
}

} // namespace directmetal::d3d11
