#include <directmetal/version.hpp>

#include <gtest/gtest.h>

TEST(Version, MatchesProjectVersion) {
    EXPECT_EQ(directmetal::version(), DIRECTMETAL_EXPECTED_VERSION);
}
