#include "log_fixture.hpp"

namespace {

using namespace directmetal::log;
using testing_support::LogFixture;

class LogSpecTest : public LogFixture {
protected:
    static Level level_of(Component component) {
        for (int index = 0; index <= static_cast<int>(Level::off); ++index) {
            const auto level = static_cast<Level>(index);
            if (is_enabled(component, level)) {
                return level;
            }
        }
        return Level::off;
    }

    std::size_t warning_count() const {
        std::size_t count = 0;
        for (const auto& line : sink.lines) {
            count += line.level == Level::warn && line.component == Component::runtime;
        }
        return count;
    }
};

TEST_F(LogSpecTest, BareLevelSetsEveryComponent) {
    EXPECT_EQ(apply_spec("error"), 0u);
    EXPECT_EQ(level_of(Component::bridge), Level::error);
    EXPECT_EQ(level_of(Component::d3d12), Level::error);
}

TEST_F(LogSpecTest, ComponentEntryOverridesBareLevel) {
    EXPECT_EQ(apply_spec("warn,d3d11=debug,bridge=trace"), 0u);
    EXPECT_EQ(level_of(Component::runtime), Level::warn);
    EXPECT_EQ(level_of(Component::d3d11), Level::debug);
    EXPECT_EQ(level_of(Component::bridge), Level::trace);
}

TEST_F(LogSpecTest, EntriesApplyLeftToRight) {
    apply_spec("d3d11=trace,error");
    EXPECT_EQ(level_of(Component::d3d11), Level::error);
}

TEST_F(LogSpecTest, IgnoresCaseWhitespaceAndEmptyEntries) {
    EXPECT_EQ(apply_spec(" WARN , D3D11 = Debug ,,"), 0u);
    EXPECT_EQ(level_of(Component::host), Level::warn);
    EXPECT_EQ(level_of(Component::d3d11), Level::debug);
}

TEST_F(LogSpecTest, EmptySpecChangesNothing) {
    EXPECT_EQ(apply_spec(""), 0u);
    EXPECT_EQ(apply_spec(" , "), 0u);
    EXPECT_EQ(level_of(Component::host), Level::info);
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(LogSpecTest, OffDisablesComponent) {
    apply_spec("host=off");
    EXPECT_FALSE(is_enabled(Component::host, Level::error));
    EXPECT_TRUE(is_enabled(Component::backend, Level::error));
}

TEST_F(LogSpecTest, ApplyKeepsLevelsNotNamed) {
    apply_spec("com=debug");
    apply_spec("host=error");
    EXPECT_EQ(level_of(Component::com), Level::debug);
}

TEST_F(LogSpecTest, InvalidEntriesWarnAndValidOnesStillApply) {
    EXPECT_EQ(apply_spec("loud,d3d11=debug,nothing=warn,bridge=,=trace,host=a=b"), 5u);
    EXPECT_EQ(level_of(Component::d3d11), Level::debug);
    EXPECT_EQ(level_of(Component::bridge), Level::info);
    EXPECT_EQ(warning_count(), 5u);
}

TEST_F(LogSpecTest, WarningNamesEntryAndProblem) {
    apply_spec("d3d12=loud,nothing=warn");
    ASSERT_EQ(sink.lines.size(), 2u);
    EXPECT_EQ(sink.lines[0].message, "log spec: ignoring \"d3d12=loud\": unknown level");
    EXPECT_EQ(sink.lines[1].message, "log spec: ignoring \"nothing=warn\": unknown component");
}

TEST_F(LogSpecTest, WarningsAreNotHiddenByAnOffEntry) {
    apply_spec("off,typo");
    EXPECT_EQ(warning_count(), 1u);
    EXPECT_FALSE(is_enabled(Component::runtime, Level::error));
}

} // namespace
