#include "texture_desc.hpp"

#include "hresult.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace directmetal::d3d11 {
namespace {

constexpr std::string_view operation = "ID3D11Device::CreateTexture2D";

// Which CPU access flags a usage requires.
enum class CpuAccessRule : std::uint8_t { none, any, write_only };

struct UsageRule {
    D3D11_USAGE usage;
    bool can_bind;
    CpuAccessRule cpu_access;
};

// Direct3D 11's usage rules
// (https://learn.microsoft.com/windows/win32/api/d3d11/ne-d3d11-d3d11_usage).
constexpr std::array usage_rules = {
    UsageRule{D3D11_USAGE_DEFAULT, true, CpuAccessRule::none},
    UsageRule{D3D11_USAGE_IMMUTABLE, true, CpuAccessRule::none},
    UsageRule{D3D11_USAGE_DYNAMIC, true, CpuAccessRule::write_only},
    UsageRule{D3D11_USAGE_STAGING, false, CpuAccessRule::any},
};

struct ImplementedUsage {
    D3D11_USAGE usage;
    UINT bind_flags;
    UINT cpu_access_flags;
};

constexpr std::array implemented_usages = {
    ImplementedUsage{D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0},
    ImplementedUsage{D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ},
};

constexpr UINT known_cpu_access = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

bool is_cpu_access_allowed(CpuAccessRule rule, UINT flags) noexcept {
    switch (rule) {
    case CpuAccessRule::none:
        return flags == 0;
    case CpuAccessRule::any:
        return flags != 0;
    case CpuAccessRule::write_only:
        return flags == D3D11_CPU_ACCESS_WRITE;
    }
    return false;
}

bool is_valid_usage(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    const auto rule =
        std::find_if(usage_rules.begin(), usage_rules.end(),
                     [&desc](const UsageRule& item) { return item.usage == desc.Usage; });
    const bool is_known =
        rule != usage_rules.end() && (desc.CPUAccessFlags & ~known_cpu_access) == 0;
    if (!is_known) {
        return false;
    }
    const bool is_bind_allowed = rule->can_bind || desc.BindFlags == 0;
    return is_bind_allowed && is_cpu_access_allowed(rule->cpu_access, desc.CPUAccessFlags);
}

bool is_implemented_usage(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    return std::any_of(implemented_usages.begin(), implemented_usages.end(),
                       [&desc](const ImplementedUsage& item) {
                           const bool is_same_usage = item.usage == desc.Usage;
                           const bool is_same_bind = item.bind_flags == desc.BindFlags;
                           return is_same_usage && is_same_bind &&
                                  item.cpu_access_flags == desc.CPUAccessFlags;
                       });
}

UINT full_mip_chain(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    return static_cast<UINT>(std::bit_width(std::max(desc.Width, desc.Height)));
}

bool is_valid_shape(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    constexpr UINT max_size = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    const bool is_width_valid = desc.Width >= 1 && desc.Width <= max_size;
    const bool is_height_valid = desc.Height >= 1 && desc.Height <= max_size;
    if (!is_width_valid || !is_height_valid) {
        return false;
    }
    const bool is_array_valid =
        desc.ArraySize >= 1 && desc.ArraySize <= D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    const bool is_samples_valid = desc.SampleDesc.Count >= 1;
    return is_array_valid && is_samples_valid && desc.MipLevels <= full_mip_chain(desc);
}

bool has_support(const D3D11_TEXTURE2D_DESC& desc, formats::Support support) noexcept {
    const bool has_texture = (support & formats::Support::texture2d) != formats::Support::none;
    const bool is_render_target = (desc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0;
    const bool has_render_target =
        (support & formats::Support::render_target) != formats::Support::none;
    return has_texture && (has_render_target || !is_render_target);
}

// Checks Direct3D rejects with E_INVALIDARG; the format is null for an unknown DXGI_FORMAT.
HRESULT check_valid(const D3D11_TEXTURE2D_DESC& desc, const formats::FormatInfo* format,
                    const formats::DeviceFeatures& features) noexcept {
    const bool is_known_format = format != nullptr && desc.Format != DXGI_FORMAT_UNKNOWN;
    if (!is_known_format || !is_valid_shape(desc) || !is_valid_usage(desc)) {
        return E_INVALIDARG;
    }
    if (format->kind == formats::ComponentKind::typeless) {
        return S_OK;
    }
    const bool is_single_sample_quality = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality == 0;
    const formats::Support support = formats::support(desc.Format, features);
    return is_single_sample_quality && has_support(desc, support) ? S_OK : E_INVALIDARG;
}

HRESULT check_implemented(const D3D11_TEXTURE2D_DESC& desc,
                          const formats::FormatInfo& format) noexcept {
    if (!is_implemented_usage(desc)) {
        return not_implemented(operation, "this usage, bind and CPU access combination");
    }
    if (desc.SampleDesc.Count != 1 || desc.MiscFlags != 0) {
        return not_implemented(operation, "multisampling and misc flags");
    }
    const bool is_typeless = format.kind == formats::ComponentKind::typeless;
    if (is_typeless || format.substitution != formats::Substitution::none) {
        return not_implemented(operation, format.name);
    }
    return S_OK;
}

} // namespace

HRESULT plan_texture2d(const D3D11_TEXTURE2D_DESC& desc, const formats::DeviceFeatures& features,
                       TexturePlan& plan) noexcept {
    const formats::FormatInfo* format = formats::find(static_cast<std::uint32_t>(desc.Format));
    if (const HRESULT result = check_valid(desc, format, features); FAILED(result)) {
        return result;
    }
    if (const HRESULT result = check_implemented(desc, *format); FAILED(result)) {
        return result;
    }
    plan.desc = desc;
    plan.desc.MipLevels = desc.MipLevels == 0 ? full_mip_chain(desc) : desc.MipLevels;
    plan.format = format;
    return S_OK;
}

bool is_staging(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    return desc.Usage == D3D11_USAGE_STAGING;
}

backend::TextureDesc to_backend_desc(const TexturePlan& plan) noexcept {
    const D3D11_TEXTURE2D_DESC& desc = plan.desc;
    backend::TextureDesc texture{};
    texture.type =
        desc.ArraySize > 1 ? backend::TextureType::tex2d_array : backend::TextureType::tex2d;
    texture.storage = backend::StorageMode::gpu_private;
    texture.format = plan.format->pixel_format;
    texture.extent = {desc.Width, desc.Height, 1};
    texture.mip_levels = desc.MipLevels;
    texture.array_length = desc.ArraySize;
    texture.sample_count = 1;
    texture.usage = backend::TextureUsage::render_target;
    return texture;
}

} // namespace directmetal::d3d11
