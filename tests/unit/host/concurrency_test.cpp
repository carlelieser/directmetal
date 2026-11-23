#include "host_fixture.hpp"

#include <algorithm>
#include <thread>
#include <vector>

namespace directmetal::host::testing {
namespace {

using namespace bridge;

constexpr int thread_count = 8;
constexpr int rounds = 200;

TEST_F(HostTest, ConcurrentCallsGetDistinctHandles) {
    initialize();
    device = create_device();
    std::vector<std::vector<std::uint64_t>> handles(thread_count);
    std::vector<std::thread> threads;
    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([this, &seen = handles[index]] {
            for (int round = 0; round < rounds; ++round) {
                CreateTimelineParams create{device, 0, 0, {}};
                CreateBufferParams buffer{device, {64, backend::StorageMode::shared, {}}, {}};
                const bool is_created = call(FunctionCode::create_timeline, create) == 0 &&
                                        call(FunctionCode::create_buffer, buffer) == 0;
                DestroyTimelineParams destroy{create.timeline};
                if (!is_created || call(FunctionCode::destroy_timeline, destroy) != 0) {
                    return;
                }
                seen.push_back(create.timeline.value);
                seen.push_back(buffer.buffer.value);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    std::vector<std::uint64_t> all;
    for (const auto& seen : handles) {
        EXPECT_EQ(seen.size(), 2u * rounds);
        all.insert(all.end(), seen.begin(), seen.end());
    }
    std::sort(all.begin(), all.end());
    EXPECT_EQ(std::adjacent_find(all.begin(), all.end()), all.end());
    EXPECT_EQ(gpu.live_buffers, thread_count * rounds);
}

TEST_F(HostTest, ConcurrentDevicesRunIndependentStreams) {
    initialize();
    std::vector<std::thread> threads;
    std::vector<Status> results(thread_count, status_unsuccessful);
    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([this, &result = results[index]] {
            alignas(64) StreamControl own{};
            const Handle owner = create_device();
            OpenStreamParams opened{owner, bridge::to_address(&own), {}, {}};
            Chunk submitted(1);
            submitted.add(SubmitCommand{}).seal();
            SubmitStreamParams params = submitted.submission({});
            if (call(FunctionCode::open_stream, opened) != 0) {
                return;
            }
            params.stream = opened.stream;
            call(FunctionCode::submit_stream, params);
            WaitTimelineParams waited{opened.progress_timeline, 1, timeout_infinite};
            const Status status = call(FunctionCode::wait_timeline, waited);
            DestroyDeviceParams destroy{owner};
            call(FunctionCode::destroy_device, destroy);
            result = own.retired_sequence == 1 ? status : status_unsuccessful;
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(std::count(results.begin(), results.end(), status_success), thread_count);
    EXPECT_EQ(gpu.live_queues, 0);
}

TEST_F(HostTest, DestroyDeviceLeavesNoObjectCreatedConcurrently) {
    initialize();
    device = create_device();
    gpu.hold_creation();
    std::thread creator([this] {
        CreateBufferParams params{device, {64, backend::StorageMode::shared, {}}, {}};
        call(FunctionCode::create_buffer, params);
    });
    gpu.wait_creation_entered();
    std::thread destroyer([this] {
        DestroyDeviceParams params{device};
        call(FunctionCode::destroy_device, params);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    gpu.release_creation();
    creator.join();
    destroyer.join();
    EXPECT_EQ(gpu.live_buffers, 0);
    CreateBufferParams after{device, {64, backend::StorageMode::shared, {}}, {}};
    EXPECT_EQ(call(FunctionCode::create_buffer, after), status_invalid_handle);
}

} // namespace
} // namespace directmetal::host::testing
