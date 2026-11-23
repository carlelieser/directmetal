#include "metal_capabilities.hpp"

namespace directmetal::formats {
namespace metal {
namespace {

bool has_feature(const CapabilityRow& row, const DeviceFeatures& features) noexcept {
    return row.feature == nullptr || features.*row.feature;
}

} // namespace

Capability capabilities_of(backend::PixelFormat pixel_format,
                           const DeviceFeatures& features) noexcept {
    Capability capabilities = Capability::none;
    for (const CapabilityRow& row : capability_rows) {
        const bool is_available = row.pixel_format == pixel_format && has_feature(row, features);
        if (is_available) {
            capabilities = capabilities | row.capabilities;
        }
    }
    return capabilities;
}

} // namespace metal

PixelFormatSupport pixel_format_support(backend::PixelFormat pixel_format,
                                        const DeviceFeatures& features) noexcept {
    using metal::Capability;
    const Capability capabilities = metal::capabilities_of(pixel_format, features);
    const auto has = [capabilities](Capability required) {
        return metal::has_all(capabilities, required);
    };
    return {has(Capability::texture), has(Capability::filter), has(Capability::write),
            has(Capability::render),  has(Capability::blend),  has(Capability::msaa),
            has(Capability::resolve)};
}

} // namespace directmetal::formats
