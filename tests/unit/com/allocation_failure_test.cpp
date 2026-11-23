#include <directmetal/com/private_data.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <new>
#include <string>

// While armed, allocations of at least failing_size bytes and below twice that size throw, so a
// test can fail one allocation by its size.
namespace {

std::atomic<std::size_t> failing_size{0};

} // namespace

void* operator new(std::size_t size) {
    const std::size_t armed = failing_size.load();
    const bool is_in_window = size >= armed && size < 2 * armed;
    if (armed != 0 && is_in_window) {
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

using directmetal::com::PrivateDataStore;

// https://learn.microsoft.com/windows/win32/direct3d11/using-the-debug-layer-to-test-apps
constexpr GUID debug_name_wide_guid = {
    0x4cca5fd8, 0x921f, 0x42c8, {0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41}};

void set_wide(PrivateDataStore& store, const std::u16string& name) {
    store.set(debug_name_wide_guid, UINT(name.size() * sizeof(char16_t)), name.data());
}

// The stored bytes take 2000 bytes, outside the window; the 1000-character UTF-8 name crosses it.
TEST(AllocationFailureTest, FailedDebugNameLeavesTheStoreUnchanged) {
    PrivateDataStore store;
    store.on_debug_name_changed([](std::string_view) {});
    set_wide(store, u"old");
    const std::u16string long_name(1000, u'x');

    failing_size.store(1000);
    EXPECT_THROW(set_wide(store, long_name), std::bad_alloc);
    failing_size.store(0);
    UINT size = 0;
    store.get(debug_name_wide_guid, &size, nullptr);
    EXPECT_EQ(size, 3 * sizeof(char16_t));
    EXPECT_EQ(store.debug_name(), "old");
}

} // namespace
