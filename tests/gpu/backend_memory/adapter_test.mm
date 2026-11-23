#include <directmetal/backend/device.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <set>

namespace {

using namespace directmetal::backend;

TEST(Adapters, EnumeratesAppleFamilyGpus) {
    const auto adapters = enumerate_adapters();
    ASSERT_FALSE(adapters.empty());
    std::set<std::uint64_t> registry_ids;
    for (const Capabilities& adapter : adapters) {
        EXPECT_GE(adapter.family, GpuFamily::apple7);
        EXPECT_EQ(adapter.has_bc, 1);
        EXPECT_EQ(adapter.has_unified_memory, 1);
        EXPECT_EQ(adapter.max_texture_2d, 16384u);
        EXPECT_EQ(adapter.max_texture_3d, 2048u);
        EXPECT_GT(adapter.max_buffer_length, std::uint64_t{256} << 20);
        EXPECT_GT(adapter.recommended_working_set, 0u);
        EXPECT_EQ(adapter.name.back(), '\0');
        EXPECT_GT(std::strlen(adapter.name.data()), 0u);
        EXPECT_EQ(adapter.reserved0, 0);
        EXPECT_EQ(adapter.reserved1, 0u);
        registry_ids.insert(adapter.registry_id);
    }
    EXPECT_EQ(registry_ids.size(), adapters.size());
}

} // namespace
