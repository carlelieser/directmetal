#include <directmetal/runtime/config.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <vector>

// process_config resolves once per process, so meson runs each test in its own process.
namespace {

namespace fs = std::filesystem;
using directmetal::config::process_config;

constexpr const char* executable_directory_variable = "DIRECTMETAL_TEST_EXECUTABLE_DIRECTORY";
constexpr const char* default_file_name = "directmetal.conf";

class ProcessConfigTest : public testing::Test {
protected:
    void SetUp() override {
        std::string pattern = (fs::temp_directory_path() / "directmetal-config-XXXXXX").string();
        ASSERT_NE(mkdtemp(pattern.data()), nullptr);
        directory = pattern;
        unsetenv("DIRECTMETAL_CONFIG");
        unsetenv("DIRECTMETAL_CONFIG_FILE");
        fs::remove(default_file_path());
    }
    void TearDown() override {
        fs::remove(default_file_path());
        fs::remove_all(directory);
    }

    static fs::path default_file_path() {
        return fs::path(std::getenv(executable_directory_variable)) / default_file_name;
    }

    fs::path write_file(const fs::path& path, const std::string& text) {
        std::ofstream(path) << text;
        return path;
    }

    void use_config_file(const std::string& text) {
        const fs::path path = write_file(directory / "custom.conf", text);
        setenv("DIRECTMETAL_CONFIG_FILE", path.c_str(), 1);
    }

    fs::path directory;
};

TEST_F(ProcessConfigTest, AppliesTheSectionOfTheRunningExecutable) {
    use_config_file("[*]\nhud = false\n[app:RUNTIME_PROCESS_TESTS]\nhud = true\n");
    EXPECT_TRUE(process_config().get_bool("hud"));
}

TEST_F(ProcessConfigTest, SectionsOfOtherExecutablesDoNotApply) {
    use_config_file("[app:runtime_process_tests.exe]\nhud = true\n[app:other]\nhud = true\n");
    EXPECT_FALSE(process_config().get_bool("hud"));
}

TEST_F(ProcessConfigTest, EnvironmentOverridesBeatTheFile) {
    use_config_file("[*]\nhud = true\n");
    setenv("DIRECTMETAL_CONFIG", "hud=off", 1);
    EXPECT_FALSE(process_config().get_bool("hud"));
}

TEST_F(ProcessConfigTest, ReadsTheFileNextToTheExecutable) {
    write_file(default_file_path(), "[*]\nhud = true\n");
    EXPECT_TRUE(process_config().get_bool("hud"));
}

TEST_F(ProcessConfigTest, ExplicitFileReplacesTheDefaultFile) {
    write_file(default_file_path(), "[*]\nhud = true\n");
    use_config_file("[*]\nhud = false\n");
    EXPECT_FALSE(process_config().get_bool("hud"));
}

TEST_F(ProcessConfigTest, AbsentDefaultFileIsSilent) {
    testing::internal::CaptureStderr();
    EXPECT_FALSE(process_config().get_bool("hud"));
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
}

TEST_F(ProcessConfigTest, MissingExplicitFileWarns) {
    const fs::path missing = directory / "missing.conf";
    setenv("DIRECTMETAL_CONFIG_FILE", missing.c_str(), 1);
    testing::internal::CaptureStderr();
    EXPECT_FALSE(process_config().get_bool("hud"));
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "directmetal: warn runtime: config file \"" +
                                                          missing.string() +
                                                          "\": cannot read (not found)\n");
}

TEST_F(ProcessConfigTest, UnreadableExplicitFileWarns) {
    setenv("DIRECTMETAL_CONFIG_FILE", directory.c_str(), 1);
    testing::internal::CaptureStderr();
    EXPECT_FALSE(process_config().get_bool("hud"));
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "directmetal: warn runtime: config file \"" +
                                                          directory.string() + "\": cannot read\n");
}

TEST_F(ProcessConfigTest, ResolvesOnceAcrossThreads) {
    use_config_file("[*]\nhud = true\nbogus = 1\n");
    constexpr int thread_count = 8;
    std::atomic<bool> is_started{false};
    std::vector<const directmetal::config::Config*> seen(thread_count, nullptr);
    std::vector<std::thread> threads;
    testing::internal::CaptureStderr();
    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([&, index] {
            while (!is_started) {
            }
            seen[static_cast<std::size_t>(index)] = &process_config();
        });
    }
    is_started = true;
    for (std::thread& thread : threads) {
        thread.join();
    }
    const std::string reported = testing::internal::GetCapturedStderr();
    for (const auto* config : seen) {
        EXPECT_EQ(config, seen[0]);
    }
    EXPECT_TRUE(seen[0]->get_bool("hud"));
    EXPECT_EQ(reported,
              "directmetal: warn runtime: config file line 3: unknown option \"bogus\"\n");
}

} // namespace
