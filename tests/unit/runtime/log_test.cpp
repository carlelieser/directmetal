#include <directmetal/runtime/log.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using namespace directmetal::log;

struct Line {
    Component component;
    Level level;
    std::string message;
};

class CapturingSink : public Sink {
public:
    void write(Component component, Level level, std::string_view message) noexcept override {
        lines.push_back({component, level, std::string(message)});
    }

    std::vector<Line> lines;
};

class LogTest : public testing::Test {
protected:
    void SetUp() override { set_sink(&sink); }
    void TearDown() override {
        set_sink(nullptr);
        set_level(Component::d3d11, Level::info);
    }

    CapturingSink sink;
};

TEST_F(LogTest, DefaultLevelIsInfo) {
    EXPECT_TRUE(is_enabled(Component::backend, Level::info));
    EXPECT_TRUE(is_enabled(Component::backend, Level::error));
    EXPECT_FALSE(is_enabled(Component::backend, Level::debug));
    EXPECT_FALSE(is_enabled(Component::backend, Level::off));
}

TEST_F(LogTest, PrintFormatsEnabledMessages) {
    print(Component::d3d11, Level::warn, "{} of {}", 3, "four");
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0].component, Component::d3d11);
    EXPECT_EQ(sink.lines[0].level, Level::warn);
    EXPECT_EQ(sink.lines[0].message, "3 of four");
}

TEST_F(LogTest, PrintSkipsDisabledLevels) {
    set_level(Component::d3d11, Level::error);
    print(Component::d3d11, Level::warn, "dropped");
    print(Component::d3d11, Level::error, "kept");
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0].message, "kept");
}

TEST(LogStderr, WritesPrefixedLine) {
    testing::internal::CaptureStderr();
    write(Component::bridge, Level::error, "stream failed");
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "directmetal: error bridge: stream failed\n");
}

TEST(LogNames, NamesEveryComponent) {
    EXPECT_EQ(name(Component::runtime), "runtime");
    EXPECT_EQ(name(Component::d3d12), "d3d12");
    EXPECT_EQ(name(Level::trace), "trace");
    EXPECT_EQ(name(Level::off), "off");
}

} // namespace
