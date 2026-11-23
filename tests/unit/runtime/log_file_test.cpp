#include "log_fixture.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using namespace directmetal::log;
using testing_support::LogFixture;

std::string read_all(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::stringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

// No sink is installed, so lines reach the file or stderr.
class LogFileTest : public testing::Test {
protected:
    void SetUp() override {
        std::string pattern = (fs::temp_directory_path() / "directmetal-log-XXXXXX").string();
        ASSERT_NE(mkdtemp(pattern.data()), nullptr);
        directory = pattern;
        path = directory / "out.log";
        testing_support::reset_levels();
    }
    void TearDown() override {
        set_file("");
        unsetenv("DIRECTMETAL_LOG");
        unsetenv("DIRECTMETAL_LOG_FILE");
        testing_support::reset_levels();
        fs::remove_all(directory);
    }

    fs::path directory;
    fs::path path;
};

TEST_F(LogFileTest, WritesFormattedLinesToFile) {
    ASSERT_TRUE(set_file(path.string()));
    write(Component::d3d11, Level::warn, "first");
    print(Component::bridge, Level::error, "{} second", 2);
    EXPECT_EQ(read_all(path),
              "directmetal: warn d3d11: first\ndirectmetal: error bridge: 2 second\n");
}

TEST_F(LogFileTest, FileReplacesStderr) {
    ASSERT_TRUE(set_file(path.string()));
    testing::internal::CaptureStderr();
    write(Component::host, Level::info, "to file");
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
}

TEST_F(LogFileTest, AppendsToExistingContent) {
    std::ofstream(path) << "existing\n";
    ASSERT_TRUE(set_file(path.string()));
    write(Component::host, Level::info, "more");
    EXPECT_EQ(read_all(path), "existing\ndirectmetal: info host: more\n");
}

TEST_F(LogFileTest, EmptyPathRestoresStderr) {
    ASSERT_TRUE(set_file(path.string()));
    ASSERT_TRUE(set_file(""));
    testing::internal::CaptureStderr();
    write(Component::host, Level::info, "to stderr");
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "directmetal: info host: to stderr\n");
    EXPECT_EQ(read_all(path), "");
}

TEST_F(LogFileTest, UnopenablePathFailsAndKeepsCurrentOutput) {
    ASSERT_TRUE(set_file(path.string()));
    testing::internal::CaptureStderr();
    EXPECT_FALSE(set_file((directory / "missing" / "out.log").string()));
    const std::string reported = testing::internal::GetCapturedStderr();
    EXPECT_EQ(reported, "");
    write(Component::host, Level::info, "kept");
    EXPECT_EQ(read_all(path), "directmetal: warn runtime: log file \"" +
                                  (directory / "missing" / "out.log").string() +
                                  "\": cannot open for append\ndirectmetal: info host: kept\n");
}

TEST_F(LogFileTest, SinkTakesPrecedenceOverFile) {
    testing_support::CapturingSink sink;
    ASSERT_TRUE(set_file(path.string()));
    set_sink(&sink);
    write(Component::host, Level::info, "captured");
    set_sink(nullptr);
    EXPECT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(read_all(path), "");
}

TEST_F(LogFileTest, ConcurrentWritersKeepLinesWhole) {
    constexpr int thread_count = 4;
    constexpr int lines_per_thread = 200;
    ASSERT_TRUE(set_file(path.string()));
    std::vector<std::thread> threads;
    for (int id = 0; id < thread_count; ++id) {
        threads.emplace_back([id] {
            for (int line = 0; line < lines_per_thread; ++line) {
                print(Component::host, Level::info, "thread {} line {}", id, line);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    std::ifstream stream(path);
    std::string text;
    int count = 0;
    while (std::getline(stream, text)) {
        ++count;
        EXPECT_EQ(text.rfind("directmetal: info host: thread ", 0), 0u) << text;
    }
    EXPECT_EQ(count, thread_count * lines_per_thread);
}

TEST_F(LogFileTest, EnvironmentSetsLevelsAndFile) {
    setenv("DIRECTMETAL_LOG", "error,d3d11=debug", 1);
    setenv("DIRECTMETAL_LOG_FILE", path.c_str(), 1);
    configure_from_environment();
    EXPECT_TRUE(is_enabled(Component::d3d11, Level::debug));
    EXPECT_FALSE(is_enabled(Component::bridge, Level::warn));
    write(Component::d3d11, Level::debug, "from env");
    EXPECT_EQ(read_all(path), "directmetal: debug d3d11: from env\n");
}

TEST_F(LogFileTest, UnsetEnvironmentChangesNothing) {
    set_level(Component::host, Level::trace);
    configure_from_environment();
    EXPECT_TRUE(is_enabled(Component::host, Level::trace));
}

TEST_F(LogFileTest, BadEnvironmentSpecWarnsButAppliesValidEntries) {
    setenv("DIRECTMETAL_LOG", "bogus,host=trace", 1);
    testing::internal::CaptureStderr();
    configure_from_environment();
    EXPECT_EQ(testing::internal::GetCapturedStderr(),
              "directmetal: warn runtime: log spec: ignoring \"bogus\": unknown level\n");
    EXPECT_TRUE(is_enabled(Component::host, Level::trace));
}

TEST_F(LogFileTest, UnopenableEnvironmentFileKeepsStderr) {
    setenv("DIRECTMETAL_LOG_FILE", (directory / "missing" / "out.log").c_str(), 1);
    testing::internal::CaptureStderr();
    configure_from_environment();
    EXPECT_NE(testing::internal::GetCapturedStderr().find("cannot open for append"),
              std::string::npos);
}

} // namespace
