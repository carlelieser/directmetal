#pragma once

#include "device_child.hpp"
#include "staging_layout.hpp"
#include "texture_desc.hpp"

#include <directmetal/bridge/protocol.hpp>
#include <directmetal/com/ref.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace directmetal::d3d11 {

class BridgeDevice;

// A DEFAULT texture is a Metal texture. A STAGING texture is PE memory imported as a unix-side
// buffer, filled and read with buffer↔texture copies; Map returns pointers into it.
class Texture2D final : public DeviceChild<ID3D11Texture2D> {
public:
    // Answered by QueryInterface so the front end can recognize its own textures.
    static constexpr GUID identity = {
        0xeb936b28, 0x37fc, 0x4fd6, {0x95, 0xb5, 0xe4, 0x16, 0x40, 0x7a, 0x39, 0x70}};

    // Creates the unix-side object; the texture starts with one public reference.
    static HRESULT create(Device& device, const TexturePlan& plan, Texture2D*& texture);

    void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION*) noexcept override;
    void STDMETHODCALLTYPE SetEvictionPriority(UINT) noexcept override;
    UINT STDMETHODCALLTYPE GetEvictionPriority() noexcept override;
    void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE2D_DESC*) noexcept override;

    const D3D11_TEXTURE2D_DESC& desc() const noexcept;
    const formats::FormatInfo& format() const noexcept;
    // The Metal texture, or for STAGING the imported buffer.
    bridge::Handle handle() const noexcept;
    bool is_staging() const noexcept;
    UINT subresource_count() const noexcept;
    // STAGING only.
    const StagingLayout& layout() const noexcept;
    std::byte* memory() const noexcept;
    // The progress value whose completion finishes the last GPU write into the texture.
    std::uint64_t last_write() const noexcept;
    void set_last_write(std::uint64_t value) noexcept;

protected:
    void* find_interface(REFIID iid) noexcept override;

private:
    struct Staging {
        StagingLayout layout;
        std::byte* memory;
    };

    // The unix-side object is created afterwards; until then the handle is null.
    Texture2D(Device& device, const TexturePlan& plan, std::optional<Staging> staging) noexcept;
    ~Texture2D() override;

    HRESULT create_unix_object(BridgeDevice& bridge, const TexturePlan& plan);

    com::PrivateRef<Device> owner_;
    D3D11_TEXTURE2D_DESC desc_;
    const formats::FormatInfo* format_;
    bridge::Handle handle_;
    std::optional<Staging> staging_;
    std::atomic<std::uint64_t> last_write_{0};
};

} // namespace directmetal::d3d11
