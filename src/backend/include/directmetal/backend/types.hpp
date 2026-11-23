#pragma once

#include <directmetal/backend/pixel_format.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

// Plain C++ shared by the Metal backend, the bridge protocol and the format tables. Structs used on
// the wire have fixed layouts, asserted below, so i686 and x86_64 builds agree byte for byte.
namespace directmetal::backend {

enum class Status : std::int32_t {
    success,
    out_of_memory,
    invalid_argument,
    unsupported,
    device_lost,
    timeout,
};

// A value or the Status that prevented producing it. Never holds Status::success without a value.
template <typename T> class Result {
public:
    Result(T value) : value_(std::move(value)) {}
    Result(Status status) : status_(status) {}

    bool has_value() const noexcept { return value_.has_value(); }
    Status status() const noexcept { return status_; }
    T& value() & { return *value_; }
    const T& value() const& { return *value_; }
    T&& value() && { return std::move(*value_); }

private:
    std::optional<T> value_;
    Status status_ = Status::success;
};

// Ordered: a later family supports everything an earlier one does.
enum class GpuFamily : std::uint8_t { apple7, apple8, apple9, apple10 };

enum class TextureType : std::uint8_t {
    tex1d,
    tex1d_array,
    tex2d,
    tex2d_array,
    tex2d_ms,
    tex2d_ms_array,
    tex3d,
    cube,
    cube_array,
};

// Values equal MTLTextureUsage.
enum class TextureUsage : std::uint32_t {
    none = 0,
    shader_read = 0x1,
    shader_write = 0x2,
    render_target = 0x4,
    pixel_format_view = 0x10,
};

constexpr TextureUsage operator|(TextureUsage left, TextureUsage right) noexcept {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(left) |
                                     static_cast<std::uint32_t>(right));
}

constexpr TextureUsage operator&(TextureUsage left, TextureUsage right) noexcept {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(left) &
                                     static_cast<std::uint32_t>(right));
}

enum class StorageMode : std::uint8_t { shared, gpu_private, memoryless };

struct Extent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct Origin3D {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

struct SubresourceRange {
    std::uint32_t first_mip;
    std::uint32_t mip_count;
    std::uint32_t first_slice;
    std::uint32_t slice_count;
};

enum class ClearKind : std::uint8_t { float_value, uint_value, sint_value };

// Raw bits of four 32-bit channels, interpreted according to kind.
struct ClearColor {
    std::array<std::uint32_t, 4> bits;
    ClearKind kind;
    std::uint8_t reserved[3];
};

enum class ClearAspect : std::uint8_t { depth = 0x1, stencil = 0x2 };

struct DepthStencilClear {
    float depth;
    std::uint8_t stencil;
    ClearAspect aspects;
    std::uint16_t reserved;
};

struct TextureDesc {
    TextureType type;
    StorageMode storage;
    std::uint16_t reserved;
    PixelFormat format;
    Extent3D extent;
    std::uint32_t mip_levels;
    std::uint32_t array_length;
    std::uint32_t sample_count;
    TextureUsage usage;
};

struct alignas(8) BufferDesc {
    std::uint64_t size;
    StorageMode storage;
    std::uint8_t reserved[7];
};

inline constexpr std::size_t adapter_name_size = 128;

struct alignas(8) Capabilities {
    GpuFamily family;
    std::uint8_t has_bc;
    std::uint8_t has_unified_memory;
    std::uint8_t reserved0;
    std::uint32_t max_texture_2d;
    std::uint32_t max_texture_3d;
    std::uint32_t reserved1;
    std::uint64_t max_buffer_length;
    std::uint64_t recommended_working_set;
    std::uint64_t registry_id;
    // UTF-8, NUL-terminated.
    std::array<char, adapter_name_size> name;
};

static_assert(sizeof(Extent3D) == 12 && sizeof(Origin3D) == 12);
static_assert(sizeof(SubresourceRange) == 16);
static_assert(sizeof(ClearColor) == 20 && offsetof(ClearColor, kind) == 16);
static_assert(sizeof(DepthStencilClear) == 8 && offsetof(DepthStencilClear, stencil) == 4);
static_assert(offsetof(DepthStencilClear, aspects) == 5);
static_assert(sizeof(TextureDesc) == 36);
static_assert(offsetof(TextureDesc, format) == 4 && offsetof(TextureDesc, extent) == 8);
static_assert(offsetof(TextureDesc, mip_levels) == 20 && offsetof(TextureDesc, usage) == 32);
static_assert(sizeof(BufferDesc) == 16 && offsetof(BufferDesc, storage) == 8);
static_assert(sizeof(Capabilities) == 168);
static_assert(offsetof(Capabilities, max_texture_2d) == 4);
static_assert(offsetof(Capabilities, max_buffer_length) == 16);
static_assert(offsetof(Capabilities, registry_id) == 32);
static_assert(offsetof(Capabilities, name) == 40);

} // namespace directmetal::backend
