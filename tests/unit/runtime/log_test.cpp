#include "log_fixture.hpp"

#include <format>
#include <stdexcept>

struct Unformattable {};

template <> struct std::formatter<Unformattable> {
    constexpr auto parse(std::format_parse_context& context) { return context.begin(); }
    template <typename Context>
    auto format(const Unformattable&, Context&) const -> typename Context::iterator {
        throw std::runtime_error("cannot format");
    }
};

namespace {

using namespace directmetal::log;
using testing_support::LogFixture;

TEST_F(LogFixture, DefaultLevelIsInfo) {
    EXPECT_TRUE(is_enabled(Component::backend, Level::info));
    EXPECT_TRUE(is_enabled(Component::backend, Level::error));
    EXPECT_FALSE(is_enabled(Component::backend, Level::debug));
    EXPECT_FALSE(is_enabled(Component::backend, Level::off));
}

TEST_F(LogFixture, PrintFormatsEnabledMessages) {
    print(Component::d3d11, Level::warn, "{} of {}", 3, "four");
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0].component, Component::d3d11);
    EXPECT_EQ(sink.lines[0].level, Level::warn);
    EXPECT_EQ(sink.lines[0].message, "3 of four");
}

TEST_F(LogFixture, PrintDropsMessagesWhoseFormatterThrows) {
    print(Component::d3d11, Level::warn, "value {}", Unformattable{});
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0].level, Level::warn);
    EXPECT_EQ(sink.lines[0].message, "(message dropped: format error)");
}

TEST_F(LogFixture, PrintSkipsDisabledLevels) {
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
