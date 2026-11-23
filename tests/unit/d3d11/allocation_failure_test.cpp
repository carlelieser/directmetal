#include "d3d11_fixture.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

// While armed, every allocation in this test executable throws.
namespace {

std::atomic<bool> is_allocation_failing{false};

} // namespace

void* operator new(std::size_t size) {
    if (is_allocation_failing.load()) {
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

using AllocationFailureTest = D3d11Test;

// Destroying a texture records its destruction; when the stream cannot grow, the texture is leaked
// to the device's teardown instead of terminating the process.
TEST_F(AllocationFailureTest, ReleasingATextureSurvivesAFailedRecord) {
    fake_bridge::state().is_retiring = false;
    ID3D11Texture2D* source = create_texture(render_target_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    ID3D11Texture2D* staging = create_texture(staging_desc(DXGI_FORMAT_R8G8B8A8_UNORM));
    context_->CopyResource(staging, source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ASSERT_EQ(context_->Map(staging, 0, D3D11_MAP_READ, 0, &mapped), S_OK);
    is_allocation_failing = true;
    const ULONG remaining = source->Release();
    is_allocation_failing = false;
    EXPECT_EQ(remaining, 0u);
    staging->Release();
}

} // namespace
