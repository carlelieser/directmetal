#include <directmetal/com/object.hpp>

#include <gtest/gtest.h>

namespace {

using directmetal::com::RefCounts;

TEST(RefCountsTest, LastPublicReleaseHandsThePrivateReferenceToTheCaller) {
    RefCounts counts;

    EXPECT_EQ(counts.release(), 0u);
    EXPECT_TRUE(counts.release_private());
}

TEST(RefCountsTest, FirstPublicReferenceTakesThePrivateReferenceAgain) {
    RefCounts counts;
    counts.add_private_ref();
    ASSERT_EQ(counts.release(), 0u);
    ASSERT_FALSE(counts.release_private());

    EXPECT_EQ(counts.add_ref(), 1u);
    EXPECT_EQ(counts.add_ref(), 2u);
    EXPECT_EQ(counts.release(), 1u);
    EXPECT_EQ(counts.release(), 0u);
    EXPECT_FALSE(counts.release_private());
    EXPECT_TRUE(counts.release_private());
}

} // namespace
