#pragma once

#include <directmetal/runtime/log.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace directmetal::log::testing_support {

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

inline void reset_levels() {
    for (int index = 0; index < static_cast<int>(Component::count); ++index) {
        set_level(static_cast<Component>(index), Level::info);
    }
}

// Installs a capturing sink and restores the default levels and stderr afterwards.
class LogFixture : public testing::Test {
protected:
    void SetUp() override {
        reset_levels();
        set_sink(&sink);
    }
    void TearDown() override {
        set_sink(nullptr);
        reset_levels();
    }

    CapturingSink sink;
};

} // namespace directmetal::log::testing_support
