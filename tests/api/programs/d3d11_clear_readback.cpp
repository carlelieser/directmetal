// Exit test of Phase 1 (docs/plans/phase-1.md#exit-test): clears render targets of several formats
// through render-target views, copies them to staging textures and reads them back. Prints the
// HRESULT of every step and the readbacks as tightly packed hex.

#include <directmetal/api_harness/harness.hpp>
#include <directmetal/api_harness/owned.hpp>

#include <d3d11.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

namespace harness = directmetal::api_harness;
using harness::Owned;

constexpr UINT texture_size = 8;
constexpr std::size_t max_mip_levels = 2;
using Color = std::array<float, 4>;

struct Format {
    const char* name;
    DXGI_FORMAT format;
    UINT bytes_per_pixel;
};

constexpr Format rgba8_unorm = {"R8G8B8A8_UNORM", DXGI_FORMAT_R8G8B8A8_UNORM, 4};
constexpr Format rgba8_srgb = {"R8G8B8A8_UNORM_SRGB", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4};
constexpr Format bgra8_unorm = {"B8G8R8A8_UNORM", DXGI_FORMAT_B8G8R8A8_UNORM, 4};
constexpr Format rgb10a2_unorm = {"R10G10B10A2_UNORM", DXGI_FORMAT_R10G10B10A2_UNORM, 4};
constexpr Format rgba16_float = {"R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT, 8};
constexpr Format rgba32_float = {"R32G32B32A32_FLOAT", DXGI_FORMAT_R32G32B32A32_FLOAT, 16};
constexpr Format rgba32_uint = {"R32G32B32A32_UINT", DXGI_FORMAT_R32G32B32A32_UINT, 16};

struct ClearCase {
    const char* name;
    Format format;
    UINT mip_levels;
    // One clear color per mip level; each value lies well inside a rounding interval.
    std::array<Color, max_mip_levels> colors;
};

constexpr ClearCase single_mip(const Format& format, const Color& color) {
    return {format.name, format, 1, {color, Color{}}};
}

constexpr Color rgba8_color = {0.2f, 0.4f, 0.6f, 0.8f};

constexpr std::array<ClearCase, 8> clear_cases = {{
    single_mip(rgba8_unorm, rgba8_color),
    single_mip(rgba8_srgb, {0.1f, 0.15f, 0.25f, 0.8f}),
    single_mip(bgra8_unorm, rgba8_color),
    single_mip(rgb10a2_unorm, {100.0f / 1023.0f, 500.0f / 1023.0f, 900.0f / 1023.0f, 2.0f / 3.0f}),
    single_mip(rgba16_float, {0.25f, -0.5f, 1.5f, 1.0f}),
    single_mip(rgba32_float, {0.125f, -2.5f, 1024.5f, 1.0f}),
    single_mip(rgba32_uint, {1.0f, 255.0f, 65536.0f, 123456.0f}),
    {"R8G8B8A8_UNORM_MIP1", rgba8_unorm, 2, {rgba8_color, Color{0.8f, 0.6f, 0.4f, 0.2f}}},
}};

// Indexed by harness::Driver.
constexpr std::array<D3D_DRIVER_TYPE, 2> driver_types = {D3D_DRIVER_TYPE_HARDWARE,
                                                         D3D_DRIVER_TYPE_WARP};

void write_hresult(harness::JsonWriter& json, const char* step, HRESULT result) {
    json.key(step);
    json.string(harness::format_hresult(static_cast<std::int32_t>(result)));
}

D3D11_TEXTURE2D_DESC texture_desc(const ClearCase& clear_case, bool is_staging) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = texture_size;
    desc.Height = texture_size;
    desc.MipLevels = clear_case.mip_levels;
    desc.ArraySize = 1;
    desc.Format = clear_case.format.format;
    desc.SampleDesc.Count = 1;
    desc.Usage = is_staging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
    desc.BindFlags = is_staging ? 0 : D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = is_staging ? D3D11_CPU_ACCESS_READ : 0;
    return desc;
}

struct Context {
    ID3D11Device* device;
    ID3D11DeviceContext* immediate;
};

struct Readback {
    HRESULT result;
    std::string hex;
};

class CaseRun {
public:
    CaseRun(const Context& context, const ClearCase& clear_case)
        : context_(context), clear_case_(clear_case) {}

    void write(harness::JsonWriter& json) {
        json.begin_object();
        json.key("name");
        json.string(clear_case_.name);
        json.key("format");
        json.string(clear_case_.format.name);
        if (SUCCEEDED(create_textures(json))) {
            HRESULT result = S_OK;
            for (UINT mip = 0; SUCCEEDED(result) && mip < clear_case_.mip_levels; ++mip) {
                result = clear_mip(mip);
                write_hresult(json, mip == 0 ? "create_rtv_mip0" : "create_rtv_mip1", result);
            }
            if (SUCCEEDED(result)) {
                context_.immediate->CopyResource(staging_.get(), target_.get());
                write_mips(json);
            }
        }
        json.end_object();
    }

private:
    HRESULT create_textures(harness::JsonWriter& json) {
        const D3D11_TEXTURE2D_DESC target_desc = texture_desc(clear_case_, false);
        const D3D11_TEXTURE2D_DESC staging_desc = texture_desc(clear_case_, true);
        HRESULT result = context_.device->CreateTexture2D(&target_desc, nullptr, target_.put());
        write_hresult(json, "create_texture", result);
        if (SUCCEEDED(result)) {
            result = context_.device->CreateTexture2D(&staging_desc, nullptr, staging_.put());
            write_hresult(json, "create_staging", result);
        }
        return result;
    }

    HRESULT clear_mip(UINT mip) {
        D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
        view_desc.Format = clear_case_.format.format;
        view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        view_desc.Texture2D.MipSlice = mip;
        Owned<ID3D11RenderTargetView> view;
        const HRESULT result =
            context_.device->CreateRenderTargetView(target_.get(), &view_desc, view.put());
        if (SUCCEEDED(result)) {
            context_.immediate->ClearRenderTargetView(view.get(), clear_case_.colors[mip].data());
        }
        return result;
    }

    Readback read_mip(UINT mip) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT result =
            context_.immediate->Map(staging_.get(), mip, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(result)) {
            return {result, {}};
        }
        const UINT extent = texture_size >> mip;
        const std::size_t row_size = std::size_t{extent} * clear_case_.format.bytes_per_pixel;
        std::vector<unsigned char> packed(row_size * extent);
        const auto* rows = static_cast<const unsigned char*>(mapped.pData);
        for (UINT row = 0; row < extent; ++row) {
            std::memcpy(packed.data() + row * row_size, rows + std::size_t{row} * mapped.RowPitch,
                        row_size);
        }
        context_.immediate->Unmap(staging_.get(), mip);
        return {result, harness::to_hex(packed.data(), packed.size())};
    }

    void write_mips(harness::JsonWriter& json) {
        json.key("mips");
        json.begin_array();
        for (UINT mip = 0; mip < clear_case_.mip_levels; ++mip) {
            const Readback readback = read_mip(mip);
            json.begin_object();
            json.key("mip");
            json.number(mip);
            write_hresult(json, "map", readback.result);
            json.key("readback");
            if (SUCCEEDED(readback.result)) {
                json.string(readback.hex);
            } else {
                json.null();
            }
            json.end_object();
        }
        json.end_array();
    }

    Context context_;
    const ClearCase& clear_case_;
    Owned<ID3D11Texture2D> target_;
    Owned<ID3D11Texture2D> staging_;
};

struct FailedCreation {
    HRESULT result;
    bool is_output_null;
};

// The output pointer starts as a sentinel, so is_output_null shows whether the call cleared it.
FailedCreation create_texture_expecting_failure(const Context& context,
                                                const D3D11_TEXTURE2D_DESC& desc) {
    alignas(ID3D11Texture2D*) static unsigned char sentinel_storage;
    auto* const sentinel = reinterpret_cast<ID3D11Texture2D*>(&sentinel_storage);
    ID3D11Texture2D* texture = sentinel;
    const HRESULT result = context.device->CreateTexture2D(&desc, nullptr, &texture);
    const bool is_created = texture != nullptr && texture != sentinel;
    if (is_created) {
        texture->Release();
    }
    return {result, texture == nullptr};
}

void run_expected_failures(harness::JsonWriter& json, const Context& context) {
    const ClearCase& clear_case = clear_cases[0];
    const D3D11_TEXTURE2D_DESC target_desc = texture_desc(clear_case, false);
    Owned<ID3D11Texture2D> target;
    json.key("expected_failures");
    json.begin_object();
    HRESULT result = context.device->CreateTexture2D(&target_desc, nullptr, target.put());
    write_hresult(json, "create_default_texture", result);
    if (SUCCEEDED(result)) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        result = context.immediate->Map(target.get(), 0, D3D11_MAP_READ, 0, &mapped);
        write_hresult(json, "map_default_texture", result);
    }
    D3D11_TEXTURE2D_DESC zero_width_desc = target_desc;
    zero_width_desc.Width = 0;
    const FailedCreation zero_width = create_texture_expecting_failure(context, zero_width_desc);
    write_hresult(json, "create_texture_zero_width", zero_width.result);
    json.key("is_zero_width_texture_null");
    json.boolean(zero_width.is_output_null);
    json.end_object();
}

HRESULT create_device(harness::Driver driver, Owned<ID3D11Device>& device,
                      D3D_FEATURE_LEVEL& level) {
    constexpr std::array<D3D_FEATURE_LEVEL, 1> levels = {D3D_FEATURE_LEVEL_11_0};
    return D3D11CreateDevice(nullptr, driver_types[static_cast<std::size_t>(driver)], nullptr, 0,
                             levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION,
                             device.put(), &level, nullptr);
}

bool run(harness::JsonWriter& json, harness::Driver driver) {
    Owned<ID3D11Device> device;
    D3D_FEATURE_LEVEL level{};
    const HRESULT result = create_device(driver, device, level);
    write_hresult(json, "create_device", result);
    if (FAILED(result)) {
        harness::log("d3d11_clear_readback: D3D11CreateDevice failed");
        return false;
    }
    json.key("feature_level");
    json.string(harness::format_hresult(static_cast<std::int32_t>(level)));
    Owned<ID3D11DeviceContext> immediate;
    device->GetImmediateContext(immediate.put());
    const Context context{device.get(), immediate.get()};
    json.key("cases");
    json.begin_array();
    for (const ClearCase& clear_case : clear_cases) {
        CaseRun(context, clear_case).write(json);
    }
    json.end_array();
    run_expected_failures(json, context);
    immediate.release();
    json.key("device_final_release");
    json.number(device.release());
    return true;
}

} // namespace

// Usage: d3d11_clear_readback [--driver hardware|warp]
int main(int argc, char** argv) {
    harness::prepare_process();
    harness::Options options;
    if (!harness::parse_options(argc, argv, options)) {
        return 2;
    }
    harness::JsonWriter json;
    json.begin_object();
    json.key("test");
    json.string("d3d11_clear_readback");
    const bool is_complete = run(json, options.driver);
    json.end_object();
    json.print();
    return is_complete ? 0 : 1;
}
