#include "fake_transport.hpp"

#include <directmetal/bridge/device_client.hpp>
#include <directmetal/runtime/log.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace {

using namespace directmetal::bridge;
using namespace directmetal::bridge::test;
namespace backend = directmetal::backend;
namespace log = directmetal::log;
using std::chrono::nanoseconds;

class CapturingSink : public log::Sink {
public:
    void write(log::Component, log::Level level, std::string_view message) noexcept override {
        lines.emplace_back(level == log::Level::warn ? "warn " : "other ");
        lines.back() += message;
    }
    std::vector<std::string> lines;
};

struct Mapping {
    Status bridge;
    backend::Status backend;
};

class ControlStatus : public testing::TestWithParam<Mapping> {
protected:
    void TearDown() override { set_call_handler({}); }
};

TEST_P(ControlStatus, MapsToBackendStatus) {
    set_call_handler([](FunctionCode, void*) { return GetParam().bridge; });
    EXPECT_EQ(query_version().status(), GetParam().backend);
}

INSTANTIATE_TEST_SUITE_P(
    Statuses, ControlStatus,
    testing::Values(Mapping{status_no_memory, backend::Status::out_of_memory},
                    Mapping{status_invalid_parameter, backend::Status::invalid_argument},
                    Mapping{status_invalid_handle, backend::Status::invalid_argument},
                    Mapping{status_not_implemented, backend::Status::unsupported},
                    Mapping{status_not_supported, backend::Status::unsupported},
                    Mapping{status_revision_mismatch, backend::Status::unsupported},
                    Mapping{status_dll_not_found, backend::Status::unsupported},
                    Mapping{status_entrypoint_not_found, backend::Status::unsupported},
                    Mapping{status_device_removed, backend::Status::device_lost},
                    Mapping{status_unsuccessful, backend::Status::device_lost},
                    Mapping{status_timeout, backend::Status::timeout}));

TEST(ControlCallLog, NamesTheFunctionAndStatus) {
    CapturingSink sink;
    log::set_sink(&sink);
    set_call_handler([](FunctionCode, void*) { return status_invalid_handle; });
    initialize("app", false);
    set_call_handler([](FunctionCode, void*) { return status_success; });
    initialize("app", false);
    set_call_handler({});
    log::set_sink(nullptr);
    EXPECT_EQ(sink.lines,
              std::vector<std::string>{"warn initialize failed with status 0xc0000008"});
}

class TimelineClientTest : public testing::Test {
protected:
    void SetUp() override {
        set_call_handler([this](FunctionCode code, void* params) {
            codes.push_back(code);
            waits.push_back(*static_cast<WaitTimelineParams*>(params));
            return reply;
        });
    }
    void TearDown() override { set_call_handler({}); }

    std::uint64_t mirror = 0;
    TimelineClient timeline{{0x77}, &mirror};
    std::vector<FunctionCode> codes;
    std::vector<WaitTimelineParams> waits;
    Status reply = status_success;
};

TEST_F(TimelineClientTest, ReadsTheMirror) {
    EXPECT_EQ(timeline.handle().value, 0x77u);
    mirror = 12;
    EXPECT_EQ(timeline.completed(), 12u);
}

TEST_F(TimelineClientTest, WaitsWithoutATransitionOnceTheMirrorReachesTheValue) {
    mirror = 5;
    EXPECT_EQ(timeline.wait(5, nanoseconds::max()), backend::Status::success);
    EXPECT_TRUE(codes.empty());
}

TEST_F(TimelineClientTest, WaitsThroughTheUnixSide) {
    mirror = 4;
    EXPECT_EQ(timeline.wait(5, nanoseconds(250)), backend::Status::success);
    EXPECT_EQ(timeline.wait(6, nanoseconds::max()), backend::Status::success);
    EXPECT_EQ(timeline.wait(7, nanoseconds(-3)), backend::Status::success);
    ASSERT_EQ(waits.size(), 3u);
    EXPECT_EQ(codes[0], FunctionCode::wait_timeline);
    EXPECT_EQ(waits[0].timeline.value, 0x77u);
    EXPECT_EQ(waits[0].value, 5u);
    EXPECT_EQ(waits[0].timeout_ns, 250u);
    EXPECT_EQ(waits[1].timeout_ns, timeout_infinite);
    EXPECT_EQ(waits[2].timeout_ns, 0u);
}

TEST_F(TimelineClientTest, ReportsTimeoutAndDeviceRemoval) {
    reply = status_timeout;
    EXPECT_EQ(timeline.wait(1, nanoseconds(10)), backend::Status::timeout);
    reply = status_device_removed;
    EXPECT_EQ(timeline.wait(1, nanoseconds(10)), backend::Status::device_lost);
}

} // namespace
