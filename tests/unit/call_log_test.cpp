#include <directmetal/frontends/markers.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

namespace fs = std::filesystem;

// Markers read DIRECTMETAL_CALL_LOG_DIR once per process, so it is set before any test runs.
class CallLogDirectory : public testing::Environment {
public:
    void SetUp() override {
        std::string pattern = (fs::temp_directory_path() / "directmetal-call-log-XXXXXX").string();
        ASSERT_NE(mkdtemp(pattern.data()), nullptr);
        ASSERT_EQ(setenv("DIRECTMETAL_CALL_LOG_DIR", pattern.c_str(), 1), 0);
    }
    void TearDown() override { fs::remove_all(std::getenv("DIRECTMETAL_CALL_LOG_DIR")); }
};

const testing::Environment* const call_log_directory =
    testing::AddGlobalTestEnvironment(new CallLogDirectory);

std::vector<fs::path> log_files() {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(std::getenv("DIRECTMETAL_CALL_LOG_DIR"))) {
        files.push_back(entry.path());
    }
    return files;
}

std::vector<std::string> logged_keys() {
    const std::vector<fs::path> files = log_files();
    EXPECT_EQ(files.size(), 1u);
    std::vector<std::string> keys;
    std::ifstream log(files.empty() ? fs::path() : files.front());
    for (std::string line; std::getline(log, line);) {
        keys.push_back(line);
    }
    return keys;
}

long count_key(const std::string& key) {
    const std::vector<std::string> keys = logged_keys();
    return std::count(keys.begin(), keys.end(), key);
}

void map(int) {
    DM_IMPL("ID3D11DeviceContext::Map");
}

void unmap() {
    DM_IMPL("ID3D11DeviceContext::Unmap");
}

void unmap_again() {
    DM_IMPL("ID3D11DeviceContext::Unmap");
}

int get_private_data() {
    DM_STUB_RETURN("ID3D11Device::GetPrivateData", -1);
}

void dispatch() {
    DM_STUB_VOID("ID3D11DeviceContext::Dispatch");
}

TEST(CallLog, FileNameHasExecutableProcessIdAndRandomSuffix) {
    map(0);
    const std::vector<fs::path> files = log_files();
    ASSERT_EQ(files.size(), 1u);
    const std::regex pattern("call_log_tests-" + std::to_string(getpid()) + "-[0-9a-f]{16}\\.log");
    EXPECT_TRUE(std::regex_match(files.front().filename().string(), pattern))
        << files.front().filename();
}

TEST(CallLog, RecordsEachSiteOncePerProcess) {
    map(1);
    map(2);
    EXPECT_EQ(count_key("ID3D11DeviceContext::Map"), 1);
}

TEST(CallLog, RecordsSameKeyOncePerSite) {
    unmap();
    unmap_again();
    unmap();
    EXPECT_EQ(count_key("ID3D11DeviceContext::Unmap"), 2);
}

TEST(CallLog, RecordsStubs) {
    EXPECT_EQ(get_private_data(), -1);
    dispatch();
    EXPECT_EQ(count_key("ID3D11Device::GetPrivateData"), 1);
    EXPECT_EQ(count_key("ID3D11DeviceContext::Dispatch"), 1);
}

void clear_state() {
    DM_IMPL("ID3D11DeviceContext::ClearState");
}

TEST(CallLog, ConcurrentFirstCallsRecordOnce) {
    std::vector<std::thread> threads;
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back(clear_state);
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(count_key("ID3D11DeviceContext::ClearState"), 1);
}

TEST(CallLog, EveryLineIsTerminated) {
    map(3);
    const std::vector<fs::path> files = log_files();
    ASSERT_EQ(files.size(), 1u);
    std::ifstream log(files.front(), std::ios::binary);
    std::stringstream contents;
    contents << log.rdbuf();
    ASSERT_FALSE(contents.str().empty());
    EXPECT_EQ(contents.str().back(), '\n');
}

} // namespace
