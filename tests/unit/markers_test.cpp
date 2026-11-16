#include <directmetal/frontends/markers.hpp>

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

namespace {

int create_buffer(int value) {
    DM_IMPL("ID3D11Device::CreateBuffer");
    return value * 2;
}

int create_query() {
    DM_STUB_RETURN("ID3D11Device::CreateQuery", -1);
}

void set_marker(int& calls) {
    ++calls;
    DM_STUB_VOID("ID3D11DeviceContext::SetMarkerInt");
}

void draw(bool& is_reached) {
    DM_STUB_VOID("ID3D11DeviceContext::Draw");
    is_reached = true;
}

unsigned get_creation_flags() {
    DM_STUB_RETURN("ID3D11Device::GetCreationFlags", 0u);
}

int get_feature_level() {
    DM_STUB_RETURN("ID3D11Device::GetFeatureLevel", 0);
}

std::size_t count_occurrences(const std::string& text, const std::string& line) {
    std::size_t count = 0;
    for (auto at = text.find(line); at != std::string::npos; at = text.find(line, at + 1)) {
        ++count;
    }
    return count;
}

TEST(Markers, ImplContinuesIntoBody) {
    EXPECT_EQ(create_buffer(21), 42);
    EXPECT_EQ(create_buffer(4), 8);
}

TEST(Markers, StubReturnReturnsValue) {
    EXPECT_EQ(create_query(), -1);
}

TEST(Markers, StubVoidReturnsBeforeBody) {
    int calls = 0;
    set_marker(calls);
    EXPECT_EQ(calls, 1);
    bool is_reached = false;
    draw(is_reached);
    EXPECT_FALSE(is_reached);
}

TEST(Markers, StubWarnsOncePerSite) {
    testing::internal::CaptureStderr();
    get_creation_flags();
    get_creation_flags();
    const std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(count_occurrences(output, "directmetal: stub ID3D11Device::GetCreationFlags\n"), 1u);
}

TEST(Markers, ConcurrentFirstCallsWarnOnce) {
    testing::internal::CaptureStderr();
    std::vector<std::thread> threads;
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([] { get_feature_level(); });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    const std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(count_occurrences(output, "directmetal: stub ID3D11Device::GetFeatureLevel\n"), 1u);
}

} // namespace
